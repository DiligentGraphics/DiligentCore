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

#include "ResourceLayoutTestCommon.hpp"
#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static Uint32 GetDeviceBits()
{
    Uint32 DeviceBits = 0;
#if D3D11_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_D3D11;
#endif
#if D3D12_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_D3D12;
#endif
#if GL_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_GL;
#endif
#if GLES_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_GLES;
#endif
#if VULKAN_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_VULKAN;
#endif
#if METAL_SUPPORTED
    DeviceBits |= 1 << RENDER_DEVICE_TYPE_METAL;
#endif
    return DeviceBits;
}

bool operator==(const RenderPassDesc& Lhs, const RenderPassDesc& Rhs)
{
    if (Lhs.AttachmentCount != Rhs.AttachmentCount ||
        Lhs.SubpassCount != Rhs.SubpassCount ||
        Lhs.DependencyCount != Rhs.DependencyCount)
        return false;

    for (Uint32 i = 0; i < Lhs.AttachmentCount; ++i)
    {
        const auto& LhsAttach = Lhs.pAttachments[i];
        const auto& RhsAttach = Rhs.pAttachments[i];
        if (!(LhsAttach == RhsAttach))
            return false;
    }

    for (Uint32 i = 0; i < Lhs.SubpassCount; ++i)
    {
        const auto& LhsSubpass = Lhs.pSubpasses[i];
        const auto& RhsSubpass = Rhs.pSubpasses[i];
        if (!(LhsSubpass == RhsSubpass))
            return false;
    }

    for (Uint32 i = 0; i < Lhs.DependencyCount; ++i)
    {
        const auto& LhsDep = Lhs.pDependencies[i];
        const auto& RhsDep = Rhs.pDependencies[i];
        if (!(LhsDep == RhsDep))
            return false;
    }
    return true;
}

bool operator==(const GraphicsPipelineDesc& Lhs, const GraphicsPipelineDesc& Rhs)
{
    if (!(Lhs.BlendDesc == Rhs.BlendDesc &&
          Lhs.SampleMask == Rhs.SampleMask &&
          Lhs.RasterizerDesc == Rhs.RasterizerDesc &&
          Lhs.DepthStencilDesc == Rhs.DepthStencilDesc &&
          Lhs.InputLayout == Rhs.InputLayout &&
          Lhs.PrimitiveTopology == Rhs.PrimitiveTopology &&
          Lhs.NumViewports == Rhs.NumViewports &&
          Lhs.NumRenderTargets == Rhs.NumRenderTargets &&
          Lhs.SubpassIndex == Rhs.SubpassIndex &&
          Lhs.ShadingRateFlags == Rhs.ShadingRateFlags &&
          Lhs.DSVFormat == Rhs.DSVFormat &&
          Lhs.SmplDesc.Count == Rhs.SmplDesc.Count &&
          Lhs.SmplDesc.Quality == Rhs.SmplDesc.Quality))
        return false;

    for (Uint32 i = 0; i < Lhs.NumRenderTargets; ++i)
    {
        if (Lhs.RTVFormats[i] != Rhs.RTVFormats[i])
            return false;
    }

    if ((Lhs.pRenderPass != nullptr) != (Rhs.pRenderPass != nullptr))
        return false;

    if (Lhs.pRenderPass)
    {
        if (!(Lhs.pRenderPass->GetDesc() == Rhs.pRenderPass->GetDesc()))
            return false;
    }
    return true;
}


TEST(ArchiveTest, ResourceSignature)
{
    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pDevice          = pEnv->GetDevice();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pDearchiver      = pDevice->GetEngineFactory()->GetDearchiver();

    if (!pDearchiver || !pArchiverFactory)
        return;

    constexpr char PRS1Name[] = "PRS archive test - 1";
    constexpr char PRS2Name[] = "PRS archive test - 2";

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_1;
    RefCntAutoPtr<IPipelineResourceSignature> pRefPRS_2;
    RefCntAutoPtr<IDeviceObjectArchive>       pArchive;
    {
        RefCntAutoPtr<ISerializationDevice> pSerializationDevice;
        pArchiverFactory->CreateSerializationDevice(&pSerializationDevice);
        ASSERT_NE(pSerializationDevice, nullptr);

        RefCntAutoPtr<IArchiver> pBuilder;
        pArchiverFactory->CreateArchiver(pSerializationDevice, &pBuilder);
        ASSERT_NE(pBuilder, nullptr);

        // PRS 1
        {
            const auto VarType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

            const PipelineResourceDesc Resources[] = //
                {
                    {SHADER_TYPE_ALL_GRAPHICS, "g_Tex2D_1", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                    {SHADER_TYPE_ALL_GRAPHICS, "g_Tex2D_2", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV, VarType},
                    {SHADER_TYPE_ALL_GRAPHICS, "ConstBuff_1", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType},
                    {SHADER_TYPE_ALL_GRAPHICS, "ConstBuff_2", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType}, //
                };

            PipelineResourceSignatureDesc PRSDesc;
            PRSDesc.Name         = PRS1Name;
            PRSDesc.Resources    = Resources;
            PRSDesc.NumResources = _countof(Resources);

            const ImmutableSamplerDesc ImmutableSamplers[] = //
                {
                    {SHADER_TYPE_ALL_GRAPHICS, "g_Sampler", SamplerDesc{}} //
                };
            PRSDesc.ImmutableSamplers    = ImmutableSamplers;
            PRSDesc.NumImmutableSamplers = _countof(ImmutableSamplers);

            ResourceSignatureArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceBits = GetDeviceBits();
            ASSERT_TRUE(pBuilder->ArchivePipelineResourceSignature(PRSDesc, ArchiveInfo));

            pDevice->CreatePipelineResourceSignature(PRSDesc, &pRefPRS_1);
            ASSERT_NE(pRefPRS_1, nullptr);
        }

        // PRS 2
        {
            const auto VarType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

            const PipelineResourceDesc Resources[] = //
                {
                    {SHADER_TYPE_COMPUTE, "g_RWTex2D", 2, SHADER_RESOURCE_TYPE_TEXTURE_UAV, VarType},
                    {SHADER_TYPE_COMPUTE, "ConstBuff", 1, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, VarType}, //
                };

            PipelineResourceSignatureDesc PRSDesc;
            PRSDesc.Name         = PRS2Name;
            PRSDesc.Resources    = Resources;
            PRSDesc.NumResources = _countof(Resources);

            ResourceSignatureArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceBits = GetDeviceBits();
            ASSERT_TRUE(pBuilder->ArchivePipelineResourceSignature(PRSDesc, ArchiveInfo));

            pDevice->CreatePipelineResourceSignature(PRSDesc, &pRefPRS_2);
            ASSERT_NE(pRefPRS_2, nullptr);
        }

        RefCntAutoPtr<IDataBlob> pBlob;
        pBuilder->SerializeToBlob(&pBlob);
        ASSERT_NE(pBlob, nullptr);

        RefCntAutoPtr<IArchive> pSource{MakeNewRCObj<ArchiveMemoryImpl>{}(pBlob)};
        pDearchiver->CreateDeviceObjectArchive(pSource, &pArchive);
        ASSERT_NE(pArchive, nullptr);
    }

    // Unpack PRS 1
    {
        ResourceSignatureUnpackInfo UnpackInfo;
        UnpackInfo.Name                     = PRS1Name;
        UnpackInfo.pArchive                 = pArchive;
        UnpackInfo.pDevice                  = pDevice;
        UnpackInfo.SRBAllocationGranularity = 10;

        RefCntAutoPtr<IPipelineResourceSignature> pUnpackedPRS;
        pDearchiver->UnpackResourceSignature(UnpackInfo, &pUnpackedPRS);
        ASSERT_NE(pUnpackedPRS, nullptr);

        ASSERT_TRUE(pUnpackedPRS->IsCompatibleWith(pRefPRS_1)); // AZ TODO: names are ignored
    }

    // Unpack PRS 2
    {
        ResourceSignatureUnpackInfo UnpackInfo;
        UnpackInfo.Name                     = PRS2Name;
        UnpackInfo.pArchive                 = pArchive;
        UnpackInfo.pDevice                  = pDevice;
        UnpackInfo.SRBAllocationGranularity = 10;

        RefCntAutoPtr<IPipelineResourceSignature> pUnpackedPRS;
        pDearchiver->UnpackResourceSignature(UnpackInfo, &pUnpackedPRS);
        ASSERT_NE(pUnpackedPRS, nullptr);

        ASSERT_TRUE(pUnpackedPRS->IsCompatibleWith(pRefPRS_2)); // AZ TODO: names are ignored
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

struct Constants
{
    float4 UVScale;
    float4 ColorScale;
    float4 NormalScale;
    float4 DepthScale;
};
} // namespace HLSL


TEST(ArchiveTest, GraphicsPipeline)
{
    auto* pEnv             = TestingEnvironment::GetInstance();
    auto* pDevice          = pEnv->GetDevice();
    auto* pArchiverFactory = pEnv->GetArchiverFactory();
    auto* pDearchiver      = pDevice->GetEngineFactory()->GetDearchiver();

    if (!pDearchiver || !pArchiverFactory)
        return;

    constexpr char PSO1Name[] = "PSO archive test - 1";
    constexpr char PSO2Name[] = "PSO archive test - 2";
    constexpr char RPName[]   = "RP archive test - 1";

    TestingEnvironment::ScopedReleaseResources AutoreleaseResources;

    auto* pSwapChain = pEnv->GetSwapChain();

    RefCntAutoPtr<ISerializationDevice> pSerializationDevice;
    pArchiverFactory->CreateSerializationDevice(&pSerializationDevice);
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
    {
        const auto VarType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

        const PipelineResourceDesc Resources[] = //
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

        const ImmutableSamplerDesc ImmutableSamplers[] = //
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
        RefCntAutoPtr<IArchiver> pBuilder;
        pArchiverFactory->CreateArchiver(pSerializationDevice, &pBuilder);
        ASSERT_NE(pBuilder, nullptr);

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler             = pEnv->GetDefaultCompiler(ShaderCI.SourceLanguage);
        ShaderCI.UseCombinedTextureSamplers = true;

        RefCntAutoPtr<IShader> pVS;
        RefCntAutoPtr<IShader> pSerializedVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Draw command test vertex shader";
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
            ShaderCI.Desc.Name       = "Draw command test pixel shader";
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

        IPipelineResourceSignature* SerializedSignatures[] = {pSerializedPRS};
        PSOCreateInfo2.ResourceSignaturesCount             = _countof(SerializedSignatures);
        PSOCreateInfo2.ppResourceSignatures                = SerializedSignatures;

        IPipelineResourceSignature* Signatures[] = {pRefPRS};
        PSOCreateInfo.ResourceSignaturesCount    = _countof(Signatures);
        PSOCreateInfo.ppResourceSignatures       = Signatures;

        // PSO 1
        {
            PSOCreateInfo2.PSODesc.Name = PSO1Name;

            PipelineStateArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceBits = GetDeviceBits();
            ASSERT_TRUE(pBuilder->ArchiveGraphicsPipelineState(PSOCreateInfo2, ArchiveInfo));

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pRefPSO_1);
            ASSERT_NE(pRefPSO_1, nullptr);
        }

        GraphicsPipeline.NumRenderTargets  = 0;
        GraphicsPipeline.RTVFormats[0]     = TEX_FORMAT_UNKNOWN;
        GraphicsPipeline2.NumRenderTargets = 0;
        GraphicsPipeline2.RTVFormats[0]    = TEX_FORMAT_UNKNOWN;

        // PSO 2
        {
            PSOCreateInfo2.PSODesc.Name   = PSO2Name;
            GraphicsPipeline2.pRenderPass = pSerializedRenderPass1;

            PipelineStateArchiveInfo ArchiveInfo;
            ArchiveInfo.DeviceBits = GetDeviceBits();
            ASSERT_TRUE(pBuilder->ArchiveGraphicsPipelineState(PSOCreateInfo2, ArchiveInfo));

            GraphicsPipeline.pRenderPass = pRenderPass1;

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pRefPSO_2);
            ASSERT_NE(pRefPSO_2, nullptr);
        }

        RefCntAutoPtr<IDataBlob> pBlob;
        pBuilder->SerializeToBlob(&pBlob);
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

        ASSERT_TRUE(pUnpackedRenderPass->GetDesc() == pRenderPass1->GetDesc());
    }

    // Unpack PSO 1
    RefCntAutoPtr<IPipelineState> pUnpackedPSO_1;
    {
        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.Name         = PSO1Name;
        UnpackInfo.pArchive     = pArchive;
        UnpackInfo.pDevice      = pDevice;
        UnpackInfo.PipelineType = PIPELINE_TYPE_GRAPHICS;

        pDearchiver->UnpackPipelineState(UnpackInfo, &pUnpackedPSO_1);
        ASSERT_NE(pUnpackedPSO_1, nullptr);

        ASSERT_TRUE(pUnpackedPSO_1->GetGraphicsPipelineDesc() == pRefPSO_1->GetGraphicsPipelineDesc());
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

        ASSERT_TRUE(pUnpackedPSO_2->GetGraphicsPipelineDesc() == pRefPSO_2->GetGraphicsPipelineDesc());
        ASSERT_TRUE(pUnpackedPSO_2->GetGraphicsPipelineDesc().pRenderPass == pUnpackedRenderPass);
    }

    auto* pContext = pEnv->GetDeviceContext();

    struct Vertex
    {
        float4 Pos;
        float3 Color;
        float2 UV;
    };
    const Vertex Vert[] =
        {
            {float4{-1.0f, -0.5f, 0.f, 1.f}, float3{1.f, 0.f, 0.f}, float2{0.0f, 0.0f}},
            {float4{-0.5f, +0.5f, 0.f, 1.f}, float3{0.f, 1.f, 0.f}, float2{0.5f, 1.0f}},
            {float4{+0.0f, -0.5f, 0.f, 1.f}, float3{0.f, 0.f, 1.f}, float2{1.0f, 0.0f}},

            {float4{+0.0f, -0.5f, 0.f, 1.f}, float3{1.f, 0.f, 0.f}, float2{0.0f, 0.0f}},
            {float4{+0.5f, +0.5f, 0.f, 1.f}, float3{0.f, 1.f, 0.f}, float2{0.5f, 1.0f}},
            {float4{+1.0f, -0.5f, 0.f, 1.f}, float3{0.f, 0.f, 1.f}, float2{1.0f, 0.0f}} //
        };
    const Vertex Triangles[] =
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

    RefCntAutoPtr<IBuffer> pConstants;
    {
        HLSL::Constants Const;
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


} // namespace
