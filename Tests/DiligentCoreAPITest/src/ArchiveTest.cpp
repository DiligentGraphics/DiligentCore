/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include <array>

#include "TestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"

#include "GraphicsAccessories.hpp"
#include "ArchiveMemoryImpl.hpp"
#include "Dearchiver.h"
#include "ShaderMacroHelper.hpp"
#include "DataBlobImpl.hpp"
#include "MemoryFileStream.hpp"

#include "ResourceLayoutTestCommon.hpp"
#include "gtest/gtest.h"

#include "InlineShaders/RayTracingTestHLSL.h"
#include "RayTracingTestConstants.hpp"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static constexpr ARCHIVE_DEVICE_DATA_FLAGS GetDeviceBits()
{
    ARCHIVE_DEVICE_DATA_FLAGS DeviceBits = ARCHIVE_DEVICE_DATA_FLAG_NONE;
#if D3D11_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_D3D11;
#endif
#if D3D12_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_D3D12;
#endif
#if GL_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_GL;
#endif
#if GLES_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_GLES;
#endif
#if VULKAN_SUPPORTED
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_VULKAN;
#endif
#if METAL_SUPPORTED
#    if PLATFORM_MACOS
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_METAL_MACOS;
#    else
    DeviceBits = DeviceBits | ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS;
#    endif
#endif
    return DeviceBits;
}

static void ArchivePRS(RefCntAutoPtr<IArchive>&                   pSource,
                       RefCntAutoPtr<IPipelineResourceSignature>& pRefPRS_1,
                       RefCntAutoPtr<IPipelineResourceSignature>& pRefPRS_2,
                       ARCHIVE_DEVICE_DATA_FLAGS                  DeviceBits)
{

    constexpr char PRS1Name[] = "PRS archive test - 1";
    constexpr char PRS2Name[] = "PRS archive test - 2";

    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pDevice          = pEnv->GetDevice();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pDearchiver      = pDevice->GetEngineFactory()->GetDearchiver();

    if (!pDearchiver || !pArchiverFactory)
        GTEST_SKIP() << "Archiver library is not loaded";

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    RefCntAutoPtr<ISerializationDevice> pSerializationDevice;
    SerializationDeviceCreateInfo       DeviceCI;
    pArchiverFactory->CreateSerializationDevice(DeviceCI, &pSerializationDevice);
    ASSERT_NE(pSerializationDevice, nullptr);

    RefCntAutoPtr<IArchiver> pArchiver;
    pArchiverFactory->CreateArchiver(pSerializationDevice, &pArchiver);
    ASSERT_NE(pArchiver, nullptr);

    // PRS 1
    {
        constexpr auto VarType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

        constexpr PipelineResourceDesc Resources[] = //
            {
                {SHADER_TYPE_ALL_GRAPHICS, "g_Tex2D_1", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "g_Tex2D_2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "ConstBuff_1", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "ConstBuff_2", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType}, //
            };

        PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name         = PRS1Name;
        PRSDesc.BindingIndex = 0;
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = _countof(Resources);

        constexpr ImmutableSamplerDesc ImmutableSamplers[] = //
            {
                {SHADER_TYPE_ALL_GRAPHICS, "g_Sampler", SamplerDesc{}} //
            };
        PRSDesc.ImmutableSamplers    = ImmutableSamplers;
        PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

        ResourceSignatureArchiveInfo ArchiveInfo;
        ArchiveInfo.DeviceFlags = DeviceBits;
        ASSERT_TRUE(pArchiver->AddPipelineResourceSignature(PRSDesc, ArchiveInfo));

        pDevice->CreatePipelineResourceSignature(PRSDesc, &pRefPRS_1);
        ASSERT_NE(pRefPRS_1, nullptr);
    }

    // PRS 2
    {
        constexpr auto VarType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

        constexpr PipelineResourceDesc Resources[] = //
            {
                {SHADER_TYPE_COMPUTE, "g_RWTex2D", 2, SHADER_RESOURCE_TYPE_TEXTURE_UAV, VarType},
                {SHADER_TYPE_COMPUTE, "ConstBuff", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType}, //
            };

        PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name         = PRS2Name;
        PRSDesc.BindingIndex = 2;
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = _countof(Resources);

        ResourceSignatureArchiveInfo ArchiveInfo;
        ArchiveInfo.DeviceFlags = DeviceBits;
        ASSERT_TRUE(pArchiver->AddPipelineResourceSignature(PRSDesc, ArchiveInfo));

        pDevice->CreatePipelineResourceSignature(PRSDesc, &pRefPRS_2);
        ASSERT_NE(pRefPRS_2, nullptr);
    }

    RefCntAutoPtr<IDataBlob> pBlob;
    pArchiver->SerializeToBlob(&pBlob);
    ASSERT_NE(pBlob, nullptr);

    pSource = RefCntAutoPtr<IArchive>{MakeNewRCObj<ArchiveMemoryImpl>{}(pBlob)};
}

static void TestPRS(IArchive*                   pSource,
                    IPipelineResourceSignature* pRefPRS_1,
                    IPipelineResourceSignature* pRefPRS_2)
{

    constexpr char PRS1Name[] = "PRS archive test - 1";
    constexpr char PRS2Name[] = "PRS archive test - 2";

    auto* pEnv        = TestingEnvironment::GetInstance();
    auto* pDevice     = pEnv->GetDevice();
    auto* pDearchiver = pDevice->GetEngineFactory()->GetDearchiver();

    RefCntAutoPtr<IDeviceObjectArchive> pArchive;
    pDearchiver->CreateDeviceObjectArchive(pSource, &pArchive);
    ASSERT_NE(pArchive, nullptr);

    // Unpack PRS 1
    {
        ResourceSignatureUnpackInfo UnpackInfo;
        UnpackInfo.Name                     = PRS1Name;
        UnpackInfo.pArchive                 = pArchive;
        UnpackInfo.pDevice                  = pDevice;
        UnpackInfo.SRBAllocationGranularity = 10;

        if (pRefPRS_1 == nullptr)
            TestingEnvironment::SetErrorAllowance(1);

        RefCntAutoPtr<IPipelineResourceSignature> pUnpackedPRS;
        pDearchiver->UnpackResourceSignature(UnpackInfo, &pUnpackedPRS);

        if (pRefPRS_1 != nullptr)
        {
            ASSERT_NE(pUnpackedPRS, nullptr);
            ASSERT_TRUE(pUnpackedPRS->IsCompatibleWith(pRefPRS_1)); // AZ TODO: names are ignored
        }
        else
        {
            ASSERT_EQ(pUnpackedPRS, nullptr);
        }
    }

    // Unpack PRS 2
    {
        ResourceSignatureUnpackInfo UnpackInfo;
        UnpackInfo.Name                     = PRS2Name;
        UnpackInfo.pArchive                 = pArchive;
        UnpackInfo.pDevice                  = pDevice;
        UnpackInfo.SRBAllocationGranularity = 10;

        if (pRefPRS_2 == nullptr)
            TestingEnvironment::SetErrorAllowance(1);

        RefCntAutoPtr<IPipelineResourceSignature> pUnpackedPRS;
        pDearchiver->UnpackResourceSignature(UnpackInfo, &pUnpackedPRS);

        if (pRefPRS_2 != nullptr)
        {
            ASSERT_NE(pUnpackedPRS, nullptr);
            ASSERT_TRUE(pUnpackedPRS->IsCompatibleWith(pRefPRS_2)); // AZ TODO: names are ignored
        }
        else
        {
            ASSERT_EQ(pUnpackedPRS, nullptr);
        }
    }
}


TEST(ArchiveTest, ResourceSignature)
{
    RefCntAutoPtr<IArchive>                   pArchive;
    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_1;
    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_2;
    ArchivePRS(pArchive, pRefPRS_1, pRefPRS_2, GetDeviceBits());
    TestPRS(pArchive, pRefPRS_1, pRefPRS_2);
}


TEST(ArchiveTest, RemoveDeviceData)
{
    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pDevice          = pEnv->GetDevice();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pDearchiver      = pDevice->GetEngineFactory()->GetDearchiver();

    if (!pDearchiver || !pArchiverFactory)
        GTEST_SKIP() << "Archiver library is not loaded";

    const auto CurrentDeviceFlag = static_cast<ARCHIVE_DEVICE_DATA_FLAGS>(1u << pDevice->GetDeviceInfo().Type);
    const auto AllDeviceFlags    = GetDeviceBits();

    if ((AllDeviceFlags & ~CurrentDeviceFlag) == 0)
        GTEST_SKIP() << "Test requires support for at least 2 backends";

    RefCntAutoPtr<IArchive> pArchive1;
    {
        RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_1;
        RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_2;
        ArchivePRS(pArchive1, pRefPRS_1, pRefPRS_2, AllDeviceFlags);
        TestPRS(pArchive1, pRefPRS_1, pRefPRS_2);
    }

    {
        auto pDataBlob  = DataBlobImpl::Create(0);
        auto pMemStream = MemoryFileStream::Create(pDataBlob);

        ASSERT_TRUE(pArchiverFactory->RemoveDeviceData(pArchive1, CurrentDeviceFlag, pMemStream));

        RefCntAutoPtr<IArchive> pArchive2{MakeNewRCObj<ArchiveMemoryImpl>{}(pDataBlob)};

        // PRS creation must fail
        TestPRS(pArchive2, nullptr, nullptr);
    }
}


TEST(ArchiveTest, AppendDeviceData)
{
    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pDevice          = pEnv->GetDevice();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pDearchiver      = pDevice->GetEngineFactory()->GetDearchiver();

    if (!pDearchiver || !pArchiverFactory)
        GTEST_SKIP() << "Archiver library is not loaded";

    const auto CurrentDeviceFlag = static_cast<ARCHIVE_DEVICE_DATA_FLAGS>(1u << pDevice->GetDeviceInfo().Type);
    auto       AllDeviceFlags    = GetDeviceBits() & ~CurrentDeviceFlag;

    if (AllDeviceFlags == 0)
        GTEST_SKIP() << "Test requires support for at least 2 backends";

    RefCntAutoPtr<IArchive> pArchive;
    for (; AllDeviceFlags != 0;)
    {
        const auto DeviceFlag = ExtractLSB(AllDeviceFlags);

        RefCntAutoPtr<IArchive>                   pArchive2;
        RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_1;
        RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_2;
        ArchivePRS(pArchive2, pRefPRS_1, pRefPRS_2, DeviceFlag);
        // PRS creation must fail
        TestPRS(pArchive2, nullptr, nullptr);

        if (pArchive != nullptr)
        {
            auto pDataBlob  = DataBlobImpl::Create(0);
            auto pMemStream = MemoryFileStream::Create(pDataBlob);

            // pArchive  - without DeviceFlag
            // pArchive2 - with DeviceFlag
            ASSERT_TRUE(pArchiverFactory->AppendDeviceData(pArchive, DeviceFlag, pArchive2, pMemStream));

            pArchive = RefCntAutoPtr<IArchive>{MakeNewRCObj<ArchiveMemoryImpl>{}(pDataBlob)};
        }
        else
        {
            pArchive = pArchive2;
        }
    }

    RefCntAutoPtr<IArchive>                   pArchive3;
    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_1;
    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_2;
    ArchivePRS(pArchive3, pRefPRS_1, pRefPRS_2, CurrentDeviceFlag);

    // Append device data
    {
        auto pDataBlob  = DataBlobImpl::Create(0);
        auto pMemStream = MemoryFileStream::Create(pDataBlob);

        // pArchive  - without CurrentDeviceFlag
        // pArchive3 - with CurrentDeviceFlag
        ASSERT_TRUE(pArchiverFactory->AppendDeviceData(pArchive, CurrentDeviceFlag, pArchive3, pMemStream));

        pArchive = RefCntAutoPtr<IArchive>{MakeNewRCObj<ArchiveMemoryImpl>{}(pDataBlob)};
        TestPRS(pArchive, pRefPRS_1, pRefPRS_2);
    }
}


namespace HLSL
{
const std::string Shared{R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float3 Color : COLOR;
    float2 UV    : TEXCOORD0;
};

cbuffer cbConstants
{
    float4 UVScale;
    float4 ColorScale;
    float4 NormalScale;
    float4 DepthScale;
}
)"};

const std::string DrawTest_VS = Shared + R"(

struct VSInput
{
    float4 Pos   : ATTRIB0;
    float3 Color : ATTRIB1;
    float2 UV    : ATTRIB2;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos   = VSIn.Pos;
    PSIn.Color = VSIn.Color;
    PSIn.UV    = VSIn.UV * UVScale.xy;
}
)";

const std::string DrawTest_PS = Shared + R"(
Texture2D    g_GBuffer_Color;
SamplerState g_GBuffer_Color_sampler;
Texture2D    g_GBuffer_Normal;
SamplerState g_GBuffer_Normal_sampler;
Texture2D    g_GBuffer_Depth;
SamplerState g_GBuffer_Depth_sampler;

float4 main(in PSInput PSIn) : SV_Target
{
    float4 Color  = g_GBuffer_Color .Sample(g_GBuffer_Color_sampler,  PSIn.UV) * ColorScale;
    float4 Normal = g_GBuffer_Normal.Sample(g_GBuffer_Normal_sampler, PSIn.UV) * NormalScale;
    float4 Depth  = g_GBuffer_Depth .Sample(g_GBuffer_Depth_sampler,  PSIn.UV) * DepthScale;

    return Color + Normal + Depth + float4(PSIn.Color.rgb, 1.0);
}
)";
} // namespace HLSL


TEST(ArchiveTest, GraphicsPipeline)
{
    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pDevice          = pEnv->GetDevice();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pDearchiver      = pDevice->GetEngineFactory()->GetDearchiver();

    if (pDevice->GetDeviceInfo().Features.SeparablePrograms != DEVICE_FEATURE_STATE_ENABLED)
        GTEST_SKIP() << "Non separable programs are not supported";

    if (!pDearchiver || !pArchiverFactory)
        GTEST_SKIP() << "Archiver library is not loaded";

    constexpr char PSO1Name[] = "PSO archive test - 1";
    constexpr char PSO2Name[] = "PSO archive test - 2";
    constexpr char PSO3Name[] = "PSO archive test - 3";
    constexpr char RPName[]   = "RP archive test - 1";

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    auto* pSwapChain = pEnv->GetSwapChain();

    SerializationDeviceCreateInfo DeviceCI;
    DeviceCI.Metal.CompileForMacOS     = True;
    DeviceCI.Metal.CompileOptionsMacOS = "-sdk macosx metal -std=macos-metal2.0 -mmacos-version-min=10.0";
    DeviceCI.Metal.LinkOptionsMacOS    = "-sdk macosx metallib";
    DeviceCI.Metal.CompileForiOS       = True;
    DeviceCI.Metal.CompileOptionsiOS   = "-sdk iphoneos metal -std=ios-metal2.0 -mios-version-min=10.0";
    DeviceCI.Metal.LinkOptionsiOS      = "-sdk iphoneos metallib";

    RefCntAutoPtr<ISerializationDevice> pSerializationDevice;
    pArchiverFactory->CreateSerializationDevice(DeviceCI, &pSerializationDevice);
    ASSERT_NE(pSerializationDevice, nullptr);

    RefCntAutoPtr<IRenderPass> pRenderPass1;
    RefCntAutoPtr<IRenderPass> pSerializedRenderPass1;
    {
        auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
        ASSERT_NE(pRTV, nullptr);
        const auto& RTVDesc = pRTV->GetTexture()->GetDesc();

        RenderPassAttachmentDesc Attachments[1];
        Attachments[0].Format       = RTVDesc.Format;
        Attachments[0].SampleCount  = static_cast<Uint8>(RTVDesc.SampleCount);
        Attachments[0].InitialState = RESOURCE_STATE_RENDER_TARGET;
        Attachments[0].FinalState   = RESOURCE_STATE_RENDER_TARGET;
        Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
        Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_STORE;

        SubpassDesc Subpasses[1] = {};

        Subpasses[0].RenderTargetAttachmentCount = 1;
        AttachmentReference RTAttachmentRef{0, RESOURCE_STATE_RENDER_TARGET};
        Subpasses[0].pRenderTargetAttachments = &RTAttachmentRef;

        RenderPassDesc RPDesc;
        RPDesc.Name            = RPName;
        RPDesc.AttachmentCount = _countof(Attachments);
        RPDesc.pAttachments    = Attachments;
        RPDesc.SubpassCount    = _countof(Subpasses);
        RPDesc.pSubpasses      = Subpasses;

        pDevice->CreateRenderPass(RPDesc, &pRenderPass1);
        ASSERT_NE(pRenderPass1, nullptr);

        pSerializationDevice->CreateRenderPass(RPDesc, &pSerializedRenderPass1);
        ASSERT_NE(pSerializedRenderPass1, nullptr);
    }

    RefCntAutoPtr<IRenderPass> pRenderPass2;
    RefCntAutoPtr<IRenderPass> pSerializedRenderPass2;
    {
        auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
        auto* pDSV = pSwapChain->GetDepthBufferDSV();
        ASSERT_NE(pRTV, nullptr);
        ASSERT_NE(pDSV, nullptr);
        const auto& RTVDesc = pRTV->GetTexture()->GetDesc();
        const auto& DSVDesc = pDSV->GetTexture()->GetDesc();

        RenderPassAttachmentDesc Attachments[2];
        Attachments[0].Format       = RTVDesc.Format;
        Attachments[0].SampleCount  = static_cast<Uint8>(RTVDesc.SampleCount);
        Attachments[0].InitialState = RESOURCE_STATE_RENDER_TARGET;
        Attachments[0].FinalState   = RESOURCE_STATE_RENDER_TARGET;
        Attachments[0].LoadOp       = ATTACHMENT_LOAD_OP_DISCARD;
        Attachments[0].StoreOp      = ATTACHMENT_STORE_OP_STORE;

        Attachments[1].Format       = DSVDesc.Format;
        Attachments[1].SampleCount  = static_cast<Uint8>(DSVDesc.SampleCount);
        Attachments[1].InitialState = RESOURCE_STATE_DEPTH_WRITE;
        Attachments[1].FinalState   = RESOURCE_STATE_DEPTH_WRITE;
        Attachments[1].LoadOp       = ATTACHMENT_LOAD_OP_CLEAR;
        Attachments[1].StoreOp      = ATTACHMENT_STORE_OP_STORE;

        SubpassDesc Subpasses[1] = {};

        Subpasses[0].RenderTargetAttachmentCount = 1;
        AttachmentReference RTAttachmentRef{0, RESOURCE_STATE_RENDER_TARGET};
        Subpasses[0].pRenderTargetAttachments = &RTAttachmentRef;

        AttachmentReference DSAttachmentRef{1, RESOURCE_STATE_DEPTH_WRITE};
        Subpasses[0].pDepthStencilAttachment = &DSAttachmentRef;

        RenderPassDesc RPDesc;
        RPDesc.Name            = "Render pass 2";
        RPDesc.AttachmentCount = _countof(Attachments);
        RPDesc.pAttachments    = Attachments;
        RPDesc.SubpassCount    = _countof(Subpasses);
        RPDesc.pSubpasses      = Subpasses;

        pDevice->CreateRenderPass(RPDesc, &pRenderPass2);
        ASSERT_NE(pRenderPass2, nullptr);

        pSerializationDevice->CreateRenderPass(RPDesc, &pSerializedRenderPass2);
        ASSERT_NE(pSerializedRenderPass2, nullptr);
    }

    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS;
    RefCntAutoPtr<IPipelineResourceSignature> pSerializedPRS;
    constexpr auto                            VarType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    {
        constexpr PipelineResourceDesc Resources[] = //
            {
                {SHADER_TYPE_PIXEL, "g_GBuffer_Color", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_PIXEL, "g_GBuffer_Normal", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_PIXEL, "g_GBuffer_Depth", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                {SHADER_TYPE_ALL_GRAPHICS, "cbConstants", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType} //
            };

        PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name         = "PRS archive test - 1";
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = _countof(Resources);

        constexpr ImmutableSamplerDesc ImmutableSamplers[] = //
            {
                {SHADER_TYPE_PIXEL, "g_GBuffer_Color_sampler", SamplerDesc{}},
                {SHADER_TYPE_PIXEL, "g_GBuffer_Normal_sampler", SamplerDesc{}},
                {SHADER_TYPE_PIXEL, "g_GBuffer_Depth_sampler", SamplerDesc{}} //
            };
        PRSDesc.ImmutableSamplers    = ImmutableSamplers;
        PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

        pSerializationDevice->CreatePipelineResourceSignature(PRSDesc, GetDeviceBits(), &pSerializedPRS);
        ASSERT_NE(pSerializedPRS, nullptr);

        pDevice->CreatePipelineResourceSignature(PRSDesc, &pRefPRS);
        ASSERT_NE(pRefPRS, nullptr);
    }

    RefCntAutoPtr<IPipelineState>       pRefPSO_1;
    RefCntAutoPtr<IPipelineState>       pRefPSO_2;
    RefCntAutoPtr<IDeviceObjectArchive> pArchive;
    {
        RefCntAutoPtr<IArchiver> pArchiver;
        pArchiverFactory->CreateArchiver(pSerializationDevice, &pArchiver);
        ASSERT_NE(pArchiver, nullptr);

        ShaderMacroHelper Macros;
        Macros.AddShaderMacro("TEST_MACRO", 1u);

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler             = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
        ShaderCI.UseCombinedTextureSamplers = true;
        ShaderCI.Macros                     = Macros;

        RefCntAutoPtr<IShader> pVS;
        RefCntAutoPtr<IShader> pSerializedVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Archive test vertex shader";
            ShaderCI.Source          = HLSL::DrawTest_VS.c_str();

            pDevice->CreateShader(ShaderCI, &pVS);
            ASSERT_NE(pVS, nullptr);

            pSerializationDevice->CreateShader(ShaderCI, GetDeviceBits(), &pSerializedVS);
            ASSERT_NE(pSerializedVS, nullptr);
        }

        RefCntAutoPtr<IShader> pPS;
        RefCntAutoPtr<IShader> pSerializedPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Archive test pixel shader";
            ShaderCI.Source          = HLSL::DrawTest_PS.c_str();

            pDevice->CreateShader(ShaderCI, &pPS);
            ASSERT_NE(pPS, nullptr);

            pSerializationDevice->CreateShader(ShaderCI, GetDeviceBits(), &pSerializedPS);
            ASSERT_NE(pSerializedPS, nullptr);
        }

        GraphicsPipelineStateCreateInfo PSOCreateInfo;
        auto&                           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        PSOCreateInfo.PSODesc.PipelineType            = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipeline.NumRenderTargets             = 1;
        GraphicsPipeline.RTVFormats[0]                = pSwapChain->GetDesc().ColorBufferFormat;
        GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
        GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        const LayoutElement InstancedElems[] =
            {
                LayoutElement{0, 0, 4, VT_FLOAT32},
                LayoutElement{1, 0, 3, VT_FLOAT32},
                LayoutElement{2, 0, 2, VT_FLOAT32} //
            };

        GraphicsPipeline.InputLayout.LayoutElements = InstancedElems;
        GraphicsPipeline.InputLayout.NumElements    = _countof(InstancedElems);

        GraphicsPipelineStateCreateInfo PSOCreateInfo2    = PSOCreateInfo;
        auto&                           GraphicsPipeline2 = PSOCreateInfo2.GraphicsPipeline;

        PSOCreateInfo.pVS  = pVS;
        PSOCreateInfo.pPS  = pPS;
        PSOCreateInfo2.pVS = pSerializedVS;
        PSOCreateInfo2.pPS = pSerializedPS;

        // PSO 1
        {
            PipelineResourceLayoutDesc LayoutDesc{};

            constexpr ImmutableSamplerDesc ImmutableSamplers[] = //
                {
                    {SHADER_TYPE_PIXEL, "g_GBuffer_Color", SamplerDesc{}},
                    {SHADER_TYPE_PIXEL, "g_GBuffer_Normal", SamplerDesc{}},
                    {SHADER_TYPE_PIXEL, "g_GBuffer_Depth", SamplerDesc{}} //
                };
            LayoutDesc.ImmutableSamplers    = ImmutableSamplers;
            LayoutDesc.NumImmutableSamplers = _countof(ImmutableSamplers);
            LayoutDesc.DefaultVariableType  = VarType;

            PSOCreateInfo2.PSODesc.Name           = PSO1Name;
            PSOCreateInfo2.PSODesc.ResourceLayout = LayoutDesc;

            PipelineStateArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceFlags = GetDeviceBits();
            ASSERT_TRUE(pArchiver->AddGraphicsPipelineState(PSOCreateInfo2, ArchiveInfo));

            PSOCreateInfo2.PSODesc.Name = PSO3Name;
            ASSERT_TRUE(pArchiver->AddGraphicsPipelineState(PSOCreateInfo2, ArchiveInfo));

            PSOCreateInfo.PSODesc.Name           = PSO1Name;
            PSOCreateInfo.PSODesc.ResourceLayout = LayoutDesc;

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pRefPSO_1);
            ASSERT_NE(pRefPSO_1, nullptr);

            PSOCreateInfo2.PSODesc.ResourceLayout = {};
            PSOCreateInfo.PSODesc.ResourceLayout  = {};
        }

        // PSO 2
        {
            IPipelineResourceSignature* SerializedSignatures[] = {pSerializedPRS};
            PSOCreateInfo2.ResourceSignaturesCount             = _countof(SerializedSignatures);
            PSOCreateInfo2.ppResourceSignatures                = SerializedSignatures;

            PSOCreateInfo2.PSODesc.Name        = PSO2Name;
            GraphicsPipeline2.pRenderPass      = pSerializedRenderPass1;
            GraphicsPipeline2.NumRenderTargets = 0;
            GraphicsPipeline2.RTVFormats[0]    = TEX_FORMAT_UNKNOWN;

            PipelineStateArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceFlags = GetDeviceBits();
            ASSERT_TRUE(pArchiver->AddGraphicsPipelineState(PSOCreateInfo2, ArchiveInfo));

            IPipelineResourceSignature* Signatures[] = {pRefPRS};
            PSOCreateInfo.ResourceSignaturesCount    = _countof(Signatures);
            PSOCreateInfo.ppResourceSignatures       = Signatures;

            PSOCreateInfo.PSODesc.Name        = PSO2Name;
            GraphicsPipeline.pRenderPass      = pRenderPass1;
            GraphicsPipeline.NumRenderTargets = 0;
            GraphicsPipeline.RTVFormats[0]    = TEX_FORMAT_UNKNOWN;

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pRefPSO_2);
            ASSERT_NE(pRefPSO_2, nullptr);
        }

        RefCntAutoPtr<IDataBlob> pBlob;
        pArchiver->SerializeToBlob(&pBlob);
        ASSERT_NE(pBlob, nullptr);

        RefCntAutoPtr<IArchive> pSource{MakeNewRCObj<ArchiveMemoryImpl>{}(pBlob)};
        pDearchiver->CreateDeviceObjectArchive(pSource, &pArchive);
        ASSERT_NE(pArchive, nullptr);
    }

    // Unpack Render pass
    RefCntAutoPtr<IRenderPass> pUnpackedRenderPass;
    {
        RenderPassUnpackInfo UnpackInfo;
        UnpackInfo.Name     = RPName;
        UnpackInfo.pArchive = pArchive;
        UnpackInfo.pDevice  = pDevice;

        pDearchiver->UnpackRenderPass(UnpackInfo, &pUnpackedRenderPass);
        ASSERT_NE(pUnpackedRenderPass, nullptr);

        EXPECT_EQ(pUnpackedRenderPass->GetDesc(), pRenderPass1->GetDesc());
    }

    // Unpack PSO 1
    {
        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.Name         = PSO1Name;
        UnpackInfo.pArchive     = pArchive;
        UnpackInfo.pDevice      = pDevice;
        UnpackInfo.PipelineType = PIPELINE_TYPE_GRAPHICS;

        RefCntAutoPtr<IPipelineState> pUnpackedPSO_1;
        pDearchiver->UnpackPipelineState(UnpackInfo, &pUnpackedPSO_1);
        ASSERT_NE(pUnpackedPSO_1, nullptr);

        EXPECT_EQ(pUnpackedPSO_1->GetGraphicsPipelineDesc(), pRefPSO_1->GetGraphicsPipelineDesc());
        EXPECT_EQ(pUnpackedPSO_1->GetResourceSignatureCount(), pRefPSO_1->GetResourceSignatureCount());

        // AZ TODO: OpenGL PRS have immutable samplers as resources which is not supported in comparator.
        // AZ TODO: Metal PRS in Archiver generated from SPIRV, in Engine - from reflection and may have different resource order.
        if (!pDevice->GetDeviceInfo().IsGLDevice() && !pDevice->GetDeviceInfo().IsMetalDevice())
        {
            for (Uint32 s = 0, SCnt = std::min(pUnpackedPSO_1->GetResourceSignatureCount(), pRefPSO_1->GetResourceSignatureCount()); s < SCnt; ++s)
            {
                auto* pLhsSign = pUnpackedPSO_1->GetResourceSignature(s);
                auto* pRhsSign = pRefPSO_1->GetResourceSignature(s);
                EXPECT_EQ((pLhsSign != nullptr), (pRhsSign != nullptr));
                if ((pLhsSign != nullptr) != (pRhsSign != nullptr))
                    continue;

                EXPECT_EQ(pLhsSign->GetDesc(), pRhsSign->GetDesc());
                EXPECT_TRUE(pLhsSign->IsCompatibleWith(pRhsSign));
            }
        }

        // Check default PRS cache
        RefCntAutoPtr<IPipelineState> pUnpackedPSO_3;
        UnpackInfo.Name = PSO3Name;
        pDearchiver->UnpackPipelineState(UnpackInfo, &pUnpackedPSO_3);
        ASSERT_NE(pUnpackedPSO_3, nullptr);

        EXPECT_EQ(pUnpackedPSO_3->GetResourceSignatureCount(), 1u);
        EXPECT_EQ(pUnpackedPSO_3->GetResourceSignature(0), pUnpackedPSO_1->GetResourceSignature(0)); // same objects
    }

    // Unpack PSO 2
    RefCntAutoPtr<IPipelineState> pUnpackedPSO_2;
    {
        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.Name         = PSO2Name;
        UnpackInfo.pArchive     = pArchive;
        UnpackInfo.pDevice      = pDevice;
        UnpackInfo.PipelineType = PIPELINE_TYPE_GRAPHICS;

        pDearchiver->UnpackPipelineState(UnpackInfo, &pUnpackedPSO_2);
        ASSERT_NE(pUnpackedPSO_2, nullptr);

        EXPECT_EQ(pUnpackedPSO_2->GetGraphicsPipelineDesc(), pRefPSO_2->GetGraphicsPipelineDesc());
        EXPECT_EQ(pUnpackedPSO_2->GetGraphicsPipelineDesc().pRenderPass, pUnpackedRenderPass);
        EXPECT_EQ(pUnpackedPSO_2->GetResourceSignatureCount(), pRefPSO_2->GetResourceSignatureCount());

        for (Uint32 s = 0, SCnt = std::min(pUnpackedPSO_2->GetResourceSignatureCount(), pRefPSO_2->GetResourceSignatureCount()); s < SCnt; ++s)
        {
            auto* pLhsSign = pUnpackedPSO_2->GetResourceSignature(s);
            auto* pRhsSign = pRefPSO_2->GetResourceSignature(s);
            EXPECT_EQ((pLhsSign != nullptr), (pRhsSign != nullptr));
            if ((pLhsSign != nullptr) != (pRhsSign != nullptr))
                continue;

            EXPECT_EQ(pLhsSign->GetDesc(), pRhsSign->GetDesc());
            EXPECT_TRUE(pLhsSign->IsCompatibleWith(pRhsSign));
        }
    }

    auto* pContext = pEnv->GetDeviceContext();

    struct Vertex
    {
        float4 Pos;
        float3 Color;
        float2 UV;
    };
    constexpr Vertex Vert[] =
        {
            {float4{-1.0f, -0.5f, 0.f, 1.f}, float3{1.f, 0.f, 0.f}, float2{0.0f, 0.0f}},
            {float4{-0.5f, +0.5f, 0.f, 1.f}, float3{0.f, 1.f, 0.f}, float2{0.5f, 1.0f}},
            {float4{+0.0f, -0.5f, 0.f, 1.f}, float3{0.f, 0.f, 1.f}, float2{1.0f, 0.0f}},

            {float4{+0.0f, -0.5f, 0.f, 1.f}, float3{1.f, 0.f, 0.f}, float2{0.0f, 0.0f}},
            {float4{+0.5f, +0.5f, 0.f, 1.f}, float3{0.f, 1.f, 0.f}, float2{0.5f, 1.0f}},
            {float4{+1.0f, -0.5f, 0.f, 1.f}, float3{0.f, 0.f, 1.f}, float2{1.0f, 0.0f}} //
        };
    constexpr Vertex Triangles[] =
        {
            Vert[0], Vert[1], Vert[2],
            Vert[3], Vert[4], Vert[5] //
        };

    RefCntAutoPtr<IBuffer> pVB;
    {
        BufferDesc BuffDesc;
        BuffDesc.Name      = "Vertex buffer";
        BuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        BuffDesc.Usage     = USAGE_IMMUTABLE;
        BuffDesc.Size      = sizeof(Triangles);

        BufferData InitialData{Triangles, sizeof(Triangles)};
        pDevice->CreateBuffer(BuffDesc, &InitialData, &pVB);
        ASSERT_NE(pVB, nullptr);

        StateTransitionDesc Barrier{pVB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);
    }
    IBuffer* pVBs[] = {pVB};

    std::array<RefCntAutoPtr<ITexture>, 3> GBuffer;
    {
        const Uint32 Width                    = 16;
        const Uint32 Height                   = 16;
        Uint32       InitData[Width * Height] = {};

        for (Uint32 y = 0; y < Height; ++y)
            for (Uint32 x = 0; x < Width; ++x)
                InitData[x + y * Width] = (x & 1 ? 0xFF000000 : 0) | (y & 1 ? 0x00FF0000 : 0) | 0x000000FF;

        for (size_t i = 0; i < GBuffer.size(); ++i)
        {
            GBuffer[i] = pEnv->CreateTexture("", TEX_FORMAT_RGBA8_UNORM, BIND_SHADER_RESOURCE, Width, Height, InitData);
            ASSERT_NE(GBuffer[i], nullptr);

            StateTransitionDesc Barrier{GBuffer[i], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
            pContext->TransitionResourceStates(1, &Barrier);
        }
    }

    struct Constants
    {
        float4 UVScale;
        float4 ColorScale;
        float4 NormalScale;
        float4 DepthScale;
    };
    RefCntAutoPtr<IBuffer> pConstants;
    {
        Constants Const;
        Const.UVScale     = float4{0.9f, 0.8f, 0.0f, 0.0f};
        Const.ColorScale  = float4{0.15f};
        Const.NormalScale = float4{0.2f};
        Const.DepthScale  = float4{0.1f};

        BufferDesc BuffDesc;
        BuffDesc.Name      = "Constant buffer";
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.Usage     = USAGE_IMMUTABLE;
        BuffDesc.Size      = sizeof(Const);

        BufferData InitialData{&Const, sizeof(Const)};
        pDevice->CreateBuffer(BuffDesc, &InitialData, &pConstants);
        ASSERT_NE(pConstants, nullptr);

        StateTransitionDesc Barrier{pConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);
    }

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    {
        pRefPRS->CreateShaderResourceBinding(&pSRB);
        ASSERT_NE(pSRB, nullptr);

        pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Color")->Set(GBuffer[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Normal")->Set(GBuffer[1]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Depth")->Set(GBuffer[2]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbConstants")->Set(pConstants);
    }

    RefCntAutoPtr<IFramebuffer> pFramebuffer;
    {
        auto*         pRTV        = pSwapChain->GetCurrentBackBufferRTV();
        ITextureView* pTexViews[] = {pRTV};

        FramebufferDesc FBDesc;
        FBDesc.Name            = "Framebuffer 1";
        FBDesc.pRenderPass     = pRenderPass1;
        FBDesc.AttachmentCount = _countof(pTexViews);
        FBDesc.ppAttachments   = pTexViews;
        pDevice->CreateFramebuffer(FBDesc, &pFramebuffer);
        ASSERT_NE(pFramebuffer, nullptr);
    }

    OptimizedClearValue ClearColor;
    ClearColor.SetColor(TEX_FORMAT_RGBA8_UNORM, 0.25f, 0.5f, 0.75f, 1.0f);

    // Draw reference
    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (pTestingSwapChain)
    {
        BeginRenderPassAttribs BeginRPInfo;
        BeginRPInfo.pRenderPass         = pRenderPass1;
        BeginRPInfo.pFramebuffer        = pFramebuffer;
        BeginRPInfo.ClearValueCount     = 1;
        BeginRPInfo.pClearValues        = &ClearColor;
        BeginRPInfo.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        pContext->BeginRenderPass(BeginRPInfo);

        pContext->SetPipelineState(pRefPSO_2);
        pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        pContext->SetVertexBuffers(0, 1, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
        pContext->Draw(DrawAttribs{6, DRAW_FLAG_VERIFY_ALL});

        pContext->EndRenderPass();

        // Transition to CopySrc state to use in TakeSnapshot()
        auto                pRT = pSwapChain->GetCurrentBackBufferRTV()->GetTexture();
        StateTransitionDesc Barrier{pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);

        pContext->Flush();
        pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

        pTestingSwapChain->TakeSnapshot(pRT);
    }

    // Draw
    {
        BeginRenderPassAttribs BeginRPInfo;
        BeginRPInfo.pRenderPass         = pUnpackedRenderPass;
        BeginRPInfo.pFramebuffer        = pFramebuffer;
        BeginRPInfo.ClearValueCount     = 1;
        BeginRPInfo.pClearValues        = &ClearColor;
        BeginRPInfo.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        pContext->BeginRenderPass(BeginRPInfo);

        pContext->SetPipelineState(pUnpackedPSO_2);
        pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        pContext->SetVertexBuffers(0, 1, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
        pContext->Draw(DrawAttribs{6, DRAW_FLAG_VERIFY_ALL});

        pContext->EndRenderPass();
    }

    pSwapChain->Present();
}


TEST(ArchiveTest, ComputePipeline)
{
    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pDevice          = pEnv->GetDevice();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pDearchiver      = pDevice->GetEngineFactory()->GetDearchiver();

    if (!pDearchiver || !pArchiverFactory)
        GTEST_SKIP() << "Archiver library is not loaded";

    if (!pDevice->GetDeviceInfo().Features.ComputeShaders)
        GTEST_SKIP() << "Compute shaders are not supported by device";

    constexpr char PSO1Name[] = "PSO archive test - 1";

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    auto*       pSwapChain = pEnv->GetSwapChain();
    const auto& SCDesc     = pSwapChain->GetDesc();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "Compute shader test requires testing swap chain";
    }

    RefCntAutoPtr<ISerializationDevice> pSerializationDevice;
    pArchiverFactory->CreateSerializationDevice(SerializationDeviceCreateInfo{}, &pSerializationDevice);
    ASSERT_NE(pSerializationDevice, nullptr);

    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS;
    RefCntAutoPtr<IPipelineResourceSignature> pSerializedPRS;
    {
        constexpr PipelineResourceDesc Resources[] = {{SHADER_TYPE_COMPUTE, "g_tex2DUAV", 1, SHADER_RESOURCE_TYPE_TEXTURE_UAV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};

        PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name         = "PRS archive test - 1";
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = _countof(Resources);

        pSerializationDevice->CreatePipelineResourceSignature(PRSDesc, GetDeviceBits(), &pSerializedPRS);
        ASSERT_NE(pSerializedPRS, nullptr);

        pDevice->CreatePipelineResourceSignature(PRSDesc, &pRefPRS);
        ASSERT_NE(pRefPRS, nullptr);
    }

    RefCntAutoPtr<IPipelineState>       pRefPSO;
    RefCntAutoPtr<IDeviceObjectArchive> pArchive;
    {
        RefCntAutoPtr<IArchiver> pArchiver;
        pArchiverFactory->CreateArchiver(pSerializationDevice, &pArchiver);
        ASSERT_NE(pArchiver, nullptr);

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler             = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
        ShaderCI.UseCombinedTextureSamplers = true;

        RefCntAutoPtr<IShader> pCS;
        RefCntAutoPtr<IShader> pSerializedCS;
        {
            RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
            pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);

            ShaderCI.Desc.ShaderType            = SHADER_TYPE_COMPUTE;
            ShaderCI.EntryPoint                 = "main";
            ShaderCI.Desc.Name                  = "Compute shader test";
            ShaderCI.FilePath                   = "ArchiveTest.csh";
            ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

            pDevice->CreateShader(ShaderCI, &pCS);
            ASSERT_NE(pCS, nullptr);

            pSerializationDevice->CreateShader(ShaderCI, GetDeviceBits(), &pSerializedCS);
            ASSERT_NE(pSerializedCS, nullptr);
        }
        {
            ComputePipelineStateCreateInfo PSOCreateInfo;
            PSOCreateInfo.PSODesc.Name         = PSO1Name;
            PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
            PSOCreateInfo.pCS                  = pCS;

            IPipelineResourceSignature* Signatures[] = {pRefPRS};
            PSOCreateInfo.ResourceSignaturesCount    = _countof(Signatures);
            PSOCreateInfo.ppResourceSignatures       = Signatures;

            pDevice->CreateComputePipelineState(PSOCreateInfo, &pRefPSO);
            ASSERT_NE(pRefPSO, nullptr);
        }
        {
            ComputePipelineStateCreateInfo PSOCreateInfo;
            PSOCreateInfo.PSODesc.Name         = PSO1Name;
            PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
            PSOCreateInfo.pCS                  = pSerializedCS;

            IPipelineResourceSignature* Signatures[] = {pSerializedPRS};
            PSOCreateInfo.ResourceSignaturesCount    = _countof(Signatures);
            PSOCreateInfo.ppResourceSignatures       = Signatures;

            PipelineStateArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceFlags = GetDeviceBits();
            ASSERT_TRUE(pArchiver->AddComputePipelineState(PSOCreateInfo, ArchiveInfo));
        }
        RefCntAutoPtr<IDataBlob> pBlob;
        pArchiver->SerializeToBlob(&pBlob);
        ASSERT_NE(pBlob, nullptr);

        RefCntAutoPtr<IArchive> pSource{MakeNewRCObj<ArchiveMemoryImpl>{}(pBlob)};
        pDearchiver->CreateDeviceObjectArchive(pSource, &pArchive);
        ASSERT_NE(pArchive, nullptr);
    }

    // Unpack PSO
    RefCntAutoPtr<IPipelineState> pUnpackedPSO;
    {
        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.Name         = PSO1Name;
        UnpackInfo.pArchive     = pArchive;
        UnpackInfo.pDevice      = pDevice;
        UnpackInfo.PipelineType = PIPELINE_TYPE_COMPUTE;

        pDearchiver->UnpackPipelineState(UnpackInfo, &pUnpackedPSO);
        ASSERT_NE(pUnpackedPSO, nullptr);
    }

    RefCntAutoPtr<IShaderResourceBinding> pSRB;
    pRefPRS->CreateShaderResourceBinding(&pSRB);
    ASSERT_NE(pSRB, nullptr);

    auto*      pContext = pEnv->GetDeviceContext();
    const auto Dispatch = [&](IPipelineState* pPSO, ITextureView* pTextureUAV) //
    {
        pSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_tex2DUAV")->Set(pTextureUAV);

        pContext->SetPipelineState(pPSO);
        pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DispatchComputeAttribs DispatchAttribs;
        DispatchAttribs.ThreadGroupCountX = (SCDesc.Width + 15) / 16;
        DispatchAttribs.ThreadGroupCountY = (SCDesc.Height + 15) / 16;
        pContext->DispatchCompute(DispatchAttribs);
    };

    // Dispatch reference
    Dispatch(pRefPSO, pTestingSwapChain->GetCurrentBackBufferUAV());

    ITexture*           pTexUAV = pTestingSwapChain->GetCurrentBackBufferUAV()->GetTexture();
    StateTransitionDesc Barrier{pTexUAV, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
    pContext->TransitionResourceStates(1, &Barrier);

    pContext->Flush();
    pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

    pTestingSwapChain->TakeSnapshot(pTexUAV);

    // Dispatch
    Dispatch(pUnpackedPSO, pTestingSwapChain->GetCurrentBackBufferUAV());

    pSwapChain->Present();
}


TEST(ArchiveTest, RayTracingPipeline)
{
    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pDevice          = pEnv->GetDevice();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pDearchiver      = pDevice->GetEngineFactory()->GetDearchiver();

    if (!pDearchiver || !pArchiverFactory)
        GTEST_SKIP() << "Archiver library is not loaded";

    if (!pEnv->SupportsRayTracing())
        GTEST_SKIP() << "Ray tracing shaders are not supported by device";

    constexpr char PSO1Name[] = "RT PSO archive test - 1";

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    auto* pSwapChain = pEnv->GetSwapChain();

    RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain{pSwapChain, IID_TestingSwapChain};
    if (!pTestingSwapChain)
    {
        GTEST_SKIP() << "Ray tracing shader test requires testing swap chain";
    }

    SerializationDeviceCreateInfo DeviceCI;
    DeviceCI.D3D12.ShaderVersion = Version{6, 5};
    DeviceCI.Vulkan.ApiVersion   = Version{1, 2};

    DeviceCI.AdapterInfo.RayTracing.CapFlags          = RAY_TRACING_CAP_FLAG_STANDALONE_SHADERS | RAY_TRACING_CAP_FLAG_INLINE_RAY_TRACING;
    DeviceCI.AdapterInfo.RayTracing.MaxRecursionDepth = 32;

    RefCntAutoPtr<ISerializationDevice> pSerializationDevice;
    pArchiverFactory->CreateSerializationDevice(DeviceCI, &pSerializationDevice);
    ASSERT_NE(pSerializationDevice, nullptr);

    const auto DeviceBits = GetDeviceBits() & (ARCHIVE_DEVICE_DATA_FLAG_D3D12 | ARCHIVE_DEVICE_DATA_FLAG_VULKAN);

    RefCntAutoPtr<IPipelineState>       pRefPSO;
    RefCntAutoPtr<IDeviceObjectArchive> pArchive;
    {
        RefCntAutoPtr<IArchiver> pArchiver;
        pArchiverFactory->CreateArchiver(pSerializationDevice, &pArchiver);
        ASSERT_NE(pArchiver, nullptr);

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;
        ShaderCI.HLSLVersion    = {6, 3};
        ShaderCI.EntryPoint     = "main";

        // Create ray generation shader.
        RefCntAutoPtr<IShader> pRG;
        RefCntAutoPtr<IShader> pSerializedRG;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
            ShaderCI.Desc.Name       = "Ray tracing RG";
            ShaderCI.Source          = HLSL::RayTracingTest1_RG.c_str();
            pDevice->CreateShader(ShaderCI, &pRG);
            ASSERT_NE(pRG, nullptr);
            pSerializationDevice->CreateShader(ShaderCI, DeviceBits, &pSerializedRG);
            ASSERT_NE(pSerializedRG, nullptr);
        }

        // Create ray miss shader.
        RefCntAutoPtr<IShader> pRMiss;
        RefCntAutoPtr<IShader> pSerializedRMiss;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
            ShaderCI.Desc.Name       = "Miss shader";
            ShaderCI.Source          = HLSL::RayTracingTest1_RM.c_str();
            pDevice->CreateShader(ShaderCI, &pRMiss);
            ASSERT_NE(pRMiss, nullptr);
            pSerializationDevice->CreateShader(ShaderCI, DeviceBits, &pSerializedRMiss);
            ASSERT_NE(pSerializedRMiss, nullptr);
        }

        // Create ray closest hit shader.
        RefCntAutoPtr<IShader> pClosestHit;
        RefCntAutoPtr<IShader> pSerializedClosestHit;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
            ShaderCI.Desc.Name       = "Ray closest hit shader";
            ShaderCI.Source          = HLSL::RayTracingTest1_RCH.c_str();
            pDevice->CreateShader(ShaderCI, &pClosestHit);
            ASSERT_NE(pClosestHit, nullptr);
            pSerializationDevice->CreateShader(ShaderCI, DeviceBits, &pSerializedClosestHit);
            ASSERT_NE(pSerializedClosestHit, nullptr);
        }

        RayTracingPipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.Name         = "Ray tracing PSO";
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_RAY_TRACING;

        PSOCreateInfo.RayTracingPipeline.MaxRecursionDepth       = 1;
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

        {
            const RayTracingGeneralShaderGroup     GeneralShaders[]     = {{"Main", pRG}, {"Miss", pRMiss}};
            const RayTracingTriangleHitShaderGroup TriangleHitShaders[] = {{"HitGroup", pClosestHit}};

            PSOCreateInfo.pGeneralShaders        = GeneralShaders;
            PSOCreateInfo.GeneralShaderCount     = _countof(GeneralShaders);
            PSOCreateInfo.pTriangleHitShaders    = TriangleHitShaders;
            PSOCreateInfo.TriangleHitShaderCount = _countof(TriangleHitShaders);

            pDevice->CreateRayTracingPipelineState(PSOCreateInfo, &pRefPSO);
            ASSERT_NE(pRefPSO, nullptr);
        }
        {
            const RayTracingGeneralShaderGroup     GeneralSerializedShaders[]     = {{"Main", pSerializedRG}, {"Miss", pSerializedRMiss}};
            const RayTracingTriangleHitShaderGroup TriangleHitSerializedShaders[] = {{"HitGroup", pSerializedClosestHit}};

            PSOCreateInfo.pGeneralShaders        = GeneralSerializedShaders;
            PSOCreateInfo.GeneralShaderCount     = _countof(GeneralSerializedShaders);
            PSOCreateInfo.pTriangleHitShaders    = TriangleHitSerializedShaders;
            PSOCreateInfo.TriangleHitShaderCount = _countof(TriangleHitSerializedShaders);
            PSOCreateInfo.PSODesc.Name           = PSO1Name;

            PipelineStateArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceFlags = DeviceBits;
            ASSERT_TRUE(pArchiver->AddRayTracingPipelineState(PSOCreateInfo, ArchiveInfo));
        }
        RefCntAutoPtr<IDataBlob> pBlob;
        pArchiver->SerializeToBlob(&pBlob);
        ASSERT_NE(pBlob, nullptr);

        RefCntAutoPtr<IArchive> pSource{MakeNewRCObj<ArchiveMemoryImpl>{}(pBlob)};
        pDearchiver->CreateDeviceObjectArchive(pSource, &pArchive);
        ASSERT_NE(pArchive, nullptr);
    }

    // Unpack PSO
    RefCntAutoPtr<IPipelineState> pUnpackedPSO;
    {
        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.Name         = PSO1Name;
        UnpackInfo.pArchive     = pArchive;
        UnpackInfo.pDevice      = pDevice;
        UnpackInfo.PipelineType = PIPELINE_TYPE_RAY_TRACING;

        pDearchiver->UnpackPipelineState(UnpackInfo, &pUnpackedPSO);
        ASSERT_NE(pUnpackedPSO, nullptr);
    }

    RefCntAutoPtr<IShaderResourceBinding> pRayTracingSRB;
    pRefPSO->CreateShaderResourceBinding(&pRayTracingSRB, true);
    ASSERT_NE(pRayTracingSRB, nullptr);

    // Create BLAS & TLAS
    RefCntAutoPtr<IBottomLevelAS> pBLAS;
    RefCntAutoPtr<ITopLevelAS>    pTLAS;
    auto*                         pContext       = pEnv->GetDeviceContext();
    const Uint32                  HitGroupStride = 1;
    {
        const auto& Vertices = TestingConstants::TriangleClosestHit::Vertices;

        RefCntAutoPtr<IBuffer> pVertexBuffer;
        {
            BufferDesc BuffDesc;
            BuffDesc.Name      = "Triangle vertices";
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = sizeof(Vertices);
            pDevice->CreateBuffer(BuffDesc, nullptr, &pVertexBuffer);
            ASSERT_NE(pVertexBuffer, nullptr);

            pContext->UpdateBuffer(pVertexBuffer, 0, sizeof(Vertices), Vertices, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        // Create & Build BLAS
        {
            BLASBuildTriangleData Triangle;
            Triangle.GeometryName         = "Triangle";
            Triangle.pVertexBuffer        = pVertexBuffer;
            Triangle.VertexStride         = sizeof(Vertices[0]);
            Triangle.VertexOffset         = 0;
            Triangle.VertexCount          = _countof(Vertices);
            Triangle.VertexValueType      = VT_FLOAT32;
            Triangle.VertexComponentCount = 3;
            Triangle.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            Triangle.PrimitiveCount       = Triangle.VertexCount / 3;

            BLASTriangleDesc TriangleDesc;
            TriangleDesc.GeometryName         = Triangle.GeometryName;
            TriangleDesc.MaxVertexCount       = Triangle.VertexCount;
            TriangleDesc.VertexValueType      = Triangle.VertexValueType;
            TriangleDesc.VertexComponentCount = Triangle.VertexComponentCount;
            TriangleDesc.MaxPrimitiveCount    = Triangle.PrimitiveCount;
            TriangleDesc.IndexType            = Triangle.IndexType;

            BottomLevelASDesc ASDesc;
            ASDesc.Name          = "Triangle BLAS";
            ASDesc.pTriangles    = &TriangleDesc;
            ASDesc.TriangleCount = 1;

            pDevice->CreateBLAS(ASDesc, &pBLAS);
            ASSERT_NE(pBLAS, nullptr);

            // Create scratch buffer
            RefCntAutoPtr<IBuffer> ScratchBuffer;

            BufferDesc BuffDesc;
            BuffDesc.Name      = "BLAS Scratch Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = std::max(pBLAS->GetScratchBufferSizes().Build, pBLAS->GetScratchBufferSizes().Update);

            pDevice->CreateBuffer(BuffDesc, nullptr, &ScratchBuffer);
            ASSERT_NE(ScratchBuffer, nullptr);

            BuildBLASAttribs Attribs;
            Attribs.pBLAS                       = pBLAS;
            Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.pTriangleData               = &Triangle;
            Attribs.TriangleDataCount           = 1;
            Attribs.pScratchBuffer              = ScratchBuffer;
            Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

            pContext->BuildBLAS(Attribs);
        }

        // Create & Build TLAS
        {
            TLASBuildInstanceData Instance;
            Instance.InstanceName = "Instance";
            Instance.pBLAS        = pBLAS;
            Instance.Flags        = RAYTRACING_INSTANCE_NONE;

            // Create TLAS
            TopLevelASDesc TLASDesc;
            TLASDesc.Name             = "TLAS";
            TLASDesc.MaxInstanceCount = 1;

            pDevice->CreateTLAS(TLASDesc, &pTLAS);
            ASSERT_NE(pTLAS, nullptr);

            // Create scratch buffer
            RefCntAutoPtr<IBuffer> ScratchBuffer;

            BufferDesc BuffDesc;
            BuffDesc.Name      = "TLAS Scratch Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = pTLAS->GetScratchBufferSizes().Build;

            pDevice->CreateBuffer(BuffDesc, nullptr, &ScratchBuffer);
            ASSERT_NE(ScratchBuffer, nullptr);

            // create instance buffer
            RefCntAutoPtr<IBuffer> InstanceBuffer;

            BuffDesc.Name      = "TLAS Instance Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = TLAS_INSTANCE_DATA_SIZE;

            pDevice->CreateBuffer(BuffDesc, nullptr, &InstanceBuffer);
            ASSERT_NE(InstanceBuffer, nullptr);

            // Build
            BuildTLASAttribs Attribs;
            Attribs.pTLAS                        = pTLAS;
            Attribs.pInstances                   = &Instance;
            Attribs.InstanceCount                = 1;
            Attribs.HitGroupStride               = HitGroupStride;
            Attribs.BindingMode                  = HIT_GROUP_BINDING_MODE_PER_GEOMETRY;
            Attribs.TLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.BLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.pInstanceBuffer              = InstanceBuffer;
            Attribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.pScratchBuffer               = ScratchBuffer;
            Attribs.ScratchBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

            pContext->BuildTLAS(Attribs);
        }
    }

    const auto CreateSBT = [&](RefCntAutoPtr<IShaderBindingTable>& pSBT, IPipelineState* pPSO) //
    {
        ShaderBindingTableDesc SBTDesc;
        SBTDesc.Name = "SBT";
        SBTDesc.pPSO = pPSO;

        pDevice->CreateSBT(SBTDesc, &pSBT);
        ASSERT_NE(pSBT, nullptr);

        pSBT->BindRayGenShader("Main");
        pSBT->BindMissShader("Miss", 0);
        pSBT->BindHitGroupForGeometry(pTLAS, "Instance", "Triangle", 0, "HitGroup");

        pContext->UpdateSBT(pSBT);
    };

    RefCntAutoPtr<IShaderBindingTable> pRefPSO_SBT;
    CreateSBT(pRefPSO_SBT, pRefPSO);

    RefCntAutoPtr<IShaderBindingTable> pUnpackedPSO_SBT;
    CreateSBT(pUnpackedPSO_SBT, pUnpackedPSO);

    pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_TLAS")->Set(pTLAS);

    const auto& SCDesc    = pSwapChain->GetDesc();
    const auto  TraceRays = [&](IPipelineState* pPSO, ITextureView* pTextureUAV, IShaderBindingTable* pSBT) //
    {
        pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_ColorBuffer")->Set(pTextureUAV);

        pContext->SetPipelineState(pPSO);
        pContext->CommitShaderResources(pRayTracingSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        TraceRaysAttribs Attribs;
        Attribs.DimensionX = SCDesc.Width;
        Attribs.DimensionY = SCDesc.Height;
        Attribs.pSBT       = pSBT;

        pContext->TraceRays(Attribs);
    };

    // Reference
    TraceRays(pRefPSO, pTestingSwapChain->GetCurrentBackBufferUAV(), pRefPSO_SBT);

    ITexture*           pTexUAV = pTestingSwapChain->GetCurrentBackBufferUAV()->GetTexture();
    StateTransitionDesc Barrier{pTexUAV, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
    pContext->TransitionResourceStates(1, &Barrier);

    pContext->Flush();

    pTestingSwapChain->TakeSnapshot(pTexUAV);

    // Unpacked
    TraceRays(pUnpackedPSO, pTestingSwapChain->GetCurrentBackBufferUAV(), pUnpackedPSO_SBT);

    pSwapChain->Present();
}


TEST(ArchiveTest, ResourceSignatureBindings)
{
    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();

    if (!pArchiverFactory)
        GTEST_SKIP() << "Archiver library is not loaded";

    RefCntAutoPtr<ISerializationDevice> pSerializationDevice;
    pArchiverFactory->CreateSerializationDevice(SerializationDeviceCreateInfo{}, &pSerializationDevice);
    ASSERT_NE(pSerializationDevice, nullptr);

    for (auto AllDeviceBits = GetDeviceBits() & ~ARCHIVE_DEVICE_DATA_FLAG_METAL_IOS; AllDeviceBits != 0;)
    {
        const auto DeviceBit  = ExtractLSB(AllDeviceBits);
        const auto DeviceType = static_cast<RENDER_DEVICE_TYPE>(PlatformMisc::GetLSB(DeviceBit));

        const auto VS_PS = SHADER_TYPE_PIXEL | SHADER_TYPE_VERTEX;
        const auto PS    = SHADER_TYPE_PIXEL;
        const auto VS    = SHADER_TYPE_VERTEX;

        // PRS 1
        RefCntAutoPtr<IPipelineResourceSignature> pPRS1;
        {
            const auto VarType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

            std::vector<PipelineResourceDesc> Resources =
                {
                    // clang-format off
                    {PS,    "g_DiffuseTexs",  100, SHADER_RESOURCE_TYPE_TEXTURE_SRV,      VarType, PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY},
                    {PS,    "g_NormalTexs",   100, SHADER_RESOURCE_TYPE_TEXTURE_SRV,      VarType, PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY},
                    {VS_PS, "ConstBuff_1",      1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VarType},
                    {VS_PS, "PerObjectConst",   8, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VarType},
                    {PS,    "g_SubpassInput",   1, SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT, VarType}
                    // clang-format on
                };

            if (DeviceType == RENDER_DEVICE_TYPE_D3D12 || DeviceType == RENDER_DEVICE_TYPE_VULKAN)
                Resources.emplace_back(PS, "g_TLAS", 1, SHADER_RESOURCE_TYPE_ACCEL_STRUCT, VarType);

            PipelineResourceSignatureDesc PRSDesc;
            PRSDesc.Name         = "PRS 1";
            PRSDesc.BindingIndex = 0;
            PRSDesc.Resources    = Resources.data();
            PRSDesc.NumResources = static_cast<Uint32>(Resources.size());

            const ImmutableSamplerDesc ImmutableSamplers[] = //
                {
                    {PS, "g_Sampler", SamplerDesc{}} //
                };
            PRSDesc.ImmutableSamplers    = ImmutableSamplers;
            PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

            pSerializationDevice->CreatePipelineResourceSignature(PRSDesc, DeviceBit, &pPRS1);
            ASSERT_NE(pPRS1, nullptr);
        }

        // PRS 2
        RefCntAutoPtr<IPipelineResourceSignature> pPRS2;
        {
            const auto VarType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

            const PipelineResourceDesc Resources[] = //
                {
                    // clang-format off
                    {PS,    "g_RWTex2D",   2, SHADER_RESOURCE_TYPE_TEXTURE_UAV, VarType},
                    {VS_PS, "g_TexelBuff", 1, SHADER_RESOURCE_TYPE_BUFFER_SRV,  VarType, PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER}
                    // clang-format on
                };

            PipelineResourceSignatureDesc PRSDesc;
            PRSDesc.Name         = "PRS 2";
            PRSDesc.BindingIndex = 2;
            PRSDesc.Resources    = Resources;
            PRSDesc.NumResources = _countof(Resources);

            pSerializationDevice->CreatePipelineResourceSignature(PRSDesc, DeviceBit, &pPRS2);
            ASSERT_NE(pPRS2, nullptr);
        }

        IPipelineResourceSignature* Signatures[] = {pPRS2, pPRS1};
        Char const* const           VBNames[]    = {"VBPosition", "VBTexcoord"};

        PipelineResourceBindingAttribs Info;
        Info.ppResourceSignatures    = Signatures;
        Info.ResourceSignaturesCount = _countof(Signatures);
        Info.ShaderStages            = SHADER_TYPE_ALL_GRAPHICS;
        Info.DeviceType              = DeviceType;

        if (DeviceType == RENDER_DEVICE_TYPE_METAL)
        {
            Info.NumVertexBuffers  = _countof(VBNames);
            Info.VertexBufferNames = VBNames;
        }

        Uint32                         NumBindings = 0;
        const PipelineResourceBinding* Bindings    = nullptr;
        pSerializationDevice->GetPipelineResourceBindings(Info, NumBindings, Bindings);
        ASSERT_NE(NumBindings, 0u);
        ASSERT_NE(Bindings, nullptr);

        const auto CompareBindings = [NumBindings, Bindings](const PipelineResourceBinding* RefBindings, Uint32 Count) //
        {
            EXPECT_EQ(NumBindings, Count);
            if (NumBindings != Count)
                return;

            struct Key
            {
                HashMapStringKey Name;
                SHADER_TYPE      Stages;

                Key(const char* _Name, SHADER_TYPE _Stages) :
                    Name{_Name}, Stages{_Stages} {}

                bool operator==(const Key& Rhs) const
                {
                    return Name == Rhs.Name && Stages == Rhs.Stages;
                }

                struct Hasher
                {
                    size_t operator()(const Key& key) const
                    {
                        size_t Hash = key.Name.GetHash();
                        HashCombine(Hash, key.Stages);
                        return Hash;
                    }
                };
            };

            std::unordered_map<Key, const PipelineResourceBinding*, Key::Hasher> BindingMap;
            for (Uint32 i = 0; i < NumBindings; ++i)
                BindingMap.emplace(Key{Bindings[i].Name, Bindings[i].ShaderStages}, Bindings + i);

            for (Uint32 i = 0; i < Count; ++i)
            {
                auto Iter = BindingMap.find(Key{RefBindings[i].Name, RefBindings[i].ShaderStages});
                EXPECT_TRUE(Iter != BindingMap.end());
                if (Iter == BindingMap.end())
                    continue;

                const auto& Lhs = *Iter->second;
                const auto& Rhs = RefBindings[i];

                EXPECT_EQ(Lhs.Register, Rhs.Register);
                EXPECT_EQ(Lhs.Space, Rhs.Space);
                EXPECT_EQ(Lhs.ArraySize, Rhs.ArraySize);
                EXPECT_EQ(Lhs.ResourceType, Rhs.ResourceType);
            }
        };

        constexpr Uint32 RuntimeArray = 0;
        switch (DeviceType)
        {
            case RENDER_DEVICE_TYPE_D3D11:
            {
                const PipelineResourceBinding RefBindings[] =
                    {
                        // clang-format off
                        {"g_DiffuseTexs",  SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,  0,   0, RuntimeArray},
                        {"g_NormalTexs",   SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,  0, 100, RuntimeArray},
                        {"g_SubpassInput", SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT, PS,  0, 200, 1},
                        {"g_TexelBuff",    SHADER_RESOURCE_TYPE_BUFFER_SRV,       PS,  0, 201, 1},
                        {"ConstBuff_1",    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  PS,  0,   0, 1},
                        {"PerObjectConst", SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  PS,  0,   1, 8},
                        {"g_RWTex2D",      SHADER_RESOURCE_TYPE_TEXTURE_UAV,      PS,  0,   0, 2},
                        {"g_Sampler",      SHADER_RESOURCE_TYPE_SAMPLER,          PS,  0,   0, 1},

                        {"ConstBuff_1",    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS,  0,   0, 1},
                        {"PerObjectConst", SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS,  0,   1, 8},
                        {"g_TexelBuff",    SHADER_RESOURCE_TYPE_BUFFER_SRV,       VS,  0,   0, 1}
                        // clang-format on

                    };
                CompareBindings(RefBindings, _countof(RefBindings));
                break;
            }
            case RENDER_DEVICE_TYPE_D3D12:
            {
                const PipelineResourceBinding RefBindings[] =
                    {
                        // clang-format off
                        {"ConstBuff_1",    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS_PS, 0, 0, 1},
                        {"PerObjectConst", SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS_PS, 0, 1, 8},
                        {"g_SubpassInput", SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT, PS,    0, 0, 1},
                        {"g_TLAS",         SHADER_RESOURCE_TYPE_ACCEL_STRUCT,     PS,    0, 1, 1},
                        {"g_DiffuseTexs",  SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,    1, 0, RuntimeArray},
                        {"g_NormalTexs",   SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,    2, 0, RuntimeArray},
                        {"g_RWTex2D",      SHADER_RESOURCE_TYPE_TEXTURE_UAV,      PS,    3, 0, 2},
                        {"g_TexelBuff",    SHADER_RESOURCE_TYPE_BUFFER_SRV,       VS_PS, 3, 0, 1}
                        // clang-format on
                    };
                CompareBindings(RefBindings, _countof(RefBindings));
                break;
            }
            case RENDER_DEVICE_TYPE_GL:
            case RENDER_DEVICE_TYPE_GLES:
            {
                const PipelineResourceBinding RefBindings[] =
                    {
                        // clang-format off
                        {"g_DiffuseTexs",  SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,  0,   0, RuntimeArray},
                        {"g_NormalTexs",   SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,  0, 100, RuntimeArray},
                        {"g_SubpassInput", SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT, PS,  0, 200, 1},
                        {"g_TexelBuff",    SHADER_RESOURCE_TYPE_BUFFER_SRV,       PS,  0, 201, 1},
                        {"ConstBuff_1",    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  PS,  0,   0, 1},
                        {"PerObjectConst", SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  PS,  0,   1, 8},
                        {"g_RWTex2D",      SHADER_RESOURCE_TYPE_TEXTURE_UAV,      PS,  0,   0, 2},

                        {"ConstBuff_1",    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS,  0,   0, 1},
                        {"PerObjectConst", SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS,  0,   1, 8},
                        {"g_TexelBuff",    SHADER_RESOURCE_TYPE_BUFFER_SRV,       VS,  0, 201, 1},
                        // clang-format on
                    };
                CompareBindings(RefBindings, _countof(RefBindings));
                break;
            }
            case RENDER_DEVICE_TYPE_VULKAN:
            {
                const PipelineResourceBinding RefBindings[] =
                    {
                        // clang-format off
                        {"ConstBuff_1",    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS_PS, 0, 0, 1},
                        {"PerObjectConst", SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS_PS, 0, 1, 8},
                        {"g_DiffuseTexs",  SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,    0, 2, RuntimeArray},
                        {"g_NormalTexs",   SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,    0, 3, RuntimeArray},
                        {"g_SubpassInput", SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT, PS,    0, 4, 1},
                        {"g_TLAS",         SHADER_RESOURCE_TYPE_ACCEL_STRUCT,     PS,    0, 5, 1},
                        {"g_RWTex2D",      SHADER_RESOURCE_TYPE_TEXTURE_UAV,      PS,    1, 0, 2},
                        {"g_TexelBuff",    SHADER_RESOURCE_TYPE_BUFFER_SRV,       VS_PS, 1, 1, 1}
                        // clang-format on
                    };
                CompareBindings(RefBindings, _countof(RefBindings));
                break;
            }
            case RENDER_DEVICE_TYPE_METAL:
            {
                const PipelineResourceBinding RefBindings[] =
                    {
                        // clang-format off
                        {"g_DiffuseTexs",  SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,  0,   0, RuntimeArray},
                        {"g_NormalTexs",   SHADER_RESOURCE_TYPE_TEXTURE_SRV,      PS,  0, 100, RuntimeArray},
                        {"g_SubpassInput", SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT, PS,  0, 200, 1},
                        {"g_RWTex2D",      SHADER_RESOURCE_TYPE_TEXTURE_UAV,      PS,  0, 201, 2},
                        {"g_TexelBuff",    SHADER_RESOURCE_TYPE_BUFFER_SRV,       PS,  0, 203, 1},
                        {"ConstBuff_1",    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  PS,  0,   0, 1},
                        {"PerObjectConst", SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  PS,  0,   1, 8},

                        {"ConstBuff_1",    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS,  0,   0, 1},
                        {"PerObjectConst", SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,  VS,  0,   1, 8},
                        {"g_TexelBuff",    SHADER_RESOURCE_TYPE_BUFFER_SRV,       VS,  0,   0, 1},
                        {"VBPosition",     SHADER_RESOURCE_TYPE_BUFFER_SRV,       VS,  0,  29, 1},
                        {"VBTexcoord",     SHADER_RESOURCE_TYPE_BUFFER_SRV,       VS,  0,  30, 1}
                        // clang-format on
                    };
                CompareBindings(RefBindings, _countof(RefBindings));
                break;
            }
            default:
                GTEST_FAIL() << "Unsupported device type";
        }
    }
}

} // namespace
