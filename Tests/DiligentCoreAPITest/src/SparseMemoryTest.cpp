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

#include "TestingEnvironment.hpp"
#include "TestingSwapChainBase.hpp"

#include "gtest/gtest.h"

#include "ShaderMacroHelper.hpp"
#include "MapHelper.hpp"
#include "Align.hpp"
#include "BasicMath.hpp"

#if PLATFORM_MACOS
#    include "../../../../Graphics/GraphicsEngineMetal/interface/RenderDeviceMtl.h"
#endif

#include "InlineShaders/SparseMemoryTestHLSL.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

class SparseMemoryTest : public testing::TestWithParam<int>
{
protected:
    static void SetUpTestSuite()
    {
        auto* pEnv    = TestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        if (!pDevice->GetDeviceInfo().Features.SparseMemory)
            return;

        // Find context.
        constexpr auto QueueTypeMask = COMMAND_QUEUE_TYPE_SPARSE_BINDING;
        for (Uint32 CtxInd = 0; CtxInd < pEnv->GetNumImmediateContexts(); ++CtxInd)
        {
            auto*       Ctx  = pEnv->GetDeviceContext(CtxInd);
            const auto& Desc = Ctx->GetDesc();

            if ((Desc.QueueType & QueueTypeMask) == QueueTypeMask)
            {
                sm_pSparseBindingCtx = Ctx;
                break;
            }
        }

        if (!sm_pSparseBindingCtx)
            return;

        // Fill buffer PSO
        {
            BufferDesc BuffDesc;
            BuffDesc.Name           = "Fill buffer parameters";
            BuffDesc.Size           = sizeof(Uint32) * 4;
            BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
            BuffDesc.Usage          = USAGE_DYNAMIC;
            BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

            pDevice->CreateBuffer(BuffDesc, nullptr, &sm_pFillBufferParams);
            ASSERT_NE(sm_pFillBufferParams, nullptr);

            ShaderCreateInfo ShaderCI;
            ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
            ShaderCI.UseCombinedTextureSamplers = true;
            ShaderCI.Desc.ShaderType            = SHADER_TYPE_COMPUTE;
            ShaderCI.EntryPoint                 = "main";
            ShaderCI.Desc.Name                  = "Fill buffer CS";
            ShaderCI.Source                     = HLSL::FillBuffer_CS.c_str();
            RefCntAutoPtr<IShader> pCS;
            pDevice->CreateShader(ShaderCI, &pCS);
            ASSERT_NE(pCS, nullptr);

            ComputePipelineStateCreateInfo PSOCreateInfo;
            PSOCreateInfo.PSODesc.Name         = "Fill buffer PSO";
            PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
            PSOCreateInfo.pCS                  = pCS;

            const ShaderResourceVariableDesc Variables[] =
                {
                    {SHADER_TYPE_COMPUTE, "CB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                    {SHADER_TYPE_COMPUTE, "g_DstBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC, SHADER_VARIABLE_FLAG_NO_DYNAMIC_BUFFERS} //
                };
            PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Variables;
            PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Variables);

            pDevice->CreateComputePipelineState(PSOCreateInfo, &sm_pFillBufferPSO);
            ASSERT_NE(sm_pFillBufferPSO, nullptr);

            sm_pFillBufferPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "CB")->Set(sm_pFillBufferParams);

            sm_pFillBufferPSO->CreateShaderResourceBinding(&sm_pFillBufferSRB, true);
            ASSERT_NE(sm_pFillBufferSRB, nullptr);
        }

        // Fullscreen quad to fill 2D texture
        {
            BufferDesc BuffDesc;
            BuffDesc.Name           = "Fill texture 2D parameters";
            BuffDesc.Size           = sizeof(float4);
            BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
            BuffDesc.Usage          = USAGE_DYNAMIC;
            BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

            pDevice->CreateBuffer(BuffDesc, nullptr, &sm_pFillTexture2DParams);
            ASSERT_NE(sm_pFillTexture2DParams, nullptr);

            GraphicsPipelineStateCreateInfo PSOCreateInfo;

            auto& PSODesc          = PSOCreateInfo.PSODesc;
            auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

            PSODesc.Name = "Fill texture 2D";

            PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

            GraphicsPipeline.NumRenderTargets                     = 1;
            GraphicsPipeline.RTVFormats[0]                        = sm_TexFormat;
            GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
            GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
            GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
            GraphicsPipeline.RasterizerDesc.ScissorEnable         = True;
            GraphicsPipeline.DepthStencilDesc.DepthEnable         = False;

            ShaderCreateInfo ShaderCI;
            ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

            RefCntAutoPtr<IShader> pVS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
                ShaderCI.EntryPoint      = "main";
                ShaderCI.Desc.Name       = "Fill texture 2D PS";
                ShaderCI.Source          = HLSL::SparseMemoryTest_VS.c_str();

                pDevice->CreateShader(ShaderCI, &pVS);
                ASSERT_NE(pVS, nullptr);
            }

            RefCntAutoPtr<IShader> pPS;
            {
                ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
                ShaderCI.EntryPoint      = "main";
                ShaderCI.Desc.Name       = "Fill texture 2D PS";
                ShaderCI.Source          = HLSL::FillTexture2D_PS.c_str();

                pDevice->CreateShader(ShaderCI, &pPS);
                ASSERT_NE(pPS, nullptr);
            }

            PSOCreateInfo.pVS = pVS;
            PSOCreateInfo.pPS = pPS;

            pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &sm_pFillTexture2DPSO);
            ASSERT_NE(sm_pFillTexture2DPSO, nullptr);

            sm_pFillTexture2DPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "CB")->Set(sm_pFillTexture2DParams);

            sm_pFillTexture2DPSO->CreateShaderResourceBinding(&sm_pFillTexture2DSRB, true);
            ASSERT_NE(sm_pFillTexture2DSRB, nullptr);
        }

        // Fill texture 3D PSO
        {
            BufferDesc BuffDesc;
            BuffDesc.Name           = "Fill texture 3D parameters";
            BuffDesc.Size           = sizeof(Uint32) * 4 * 3;
            BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
            BuffDesc.Usage          = USAGE_DYNAMIC;
            BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

            pDevice->CreateBuffer(BuffDesc, nullptr, &sm_pFillTexture3DParams);
            ASSERT_NE(sm_pFillTexture3DParams, nullptr);

            ShaderCreateInfo ShaderCI;
            ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
            ShaderCI.UseCombinedTextureSamplers = true;
            ShaderCI.Desc.ShaderType            = SHADER_TYPE_COMPUTE;
            ShaderCI.EntryPoint                 = "main";
            ShaderCI.Desc.Name                  = "Fill texture 3D CS";
            ShaderCI.Source                     = HLSL::FillTexture3D_CS.c_str();
            RefCntAutoPtr<IShader> pCS;
            pDevice->CreateShader(ShaderCI, &pCS);
            ASSERT_NE(pCS, nullptr);

            ComputePipelineStateCreateInfo PSOCreateInfo;
            PSOCreateInfo.PSODesc.Name         = "Fill texture 3D PSO";
            PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
            PSOCreateInfo.pCS                  = pCS;

            const ShaderResourceVariableDesc Variables[] =
                {
                    {SHADER_TYPE_COMPUTE, "CB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                    {SHADER_TYPE_COMPUTE, "g_DstTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC} //
                };
            PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Variables;
            PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Variables);

            pDevice->CreateComputePipelineState(PSOCreateInfo, &sm_pFillTexture3DPSO);
            ASSERT_NE(sm_pFillTexture3DPSO, nullptr);

            sm_pFillTexture3DPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "CB")->Set(sm_pFillTexture3DParams);

            sm_pFillTexture3DPSO->CreateShaderResourceBinding(&sm_pFillTexture3DSRB, true);
            ASSERT_NE(sm_pFillTexture3DSRB, nullptr);
        }
    }

    static void TearDownTestSuite()
    {
        sm_pSparseBindingCtx.Release();

        sm_pFillBufferPSO.Release();
        sm_pFillBufferSRB.Release();
        sm_pFillBufferParams.Release();

        sm_pFillTexture2DPSO.Release();
        sm_pFillTexture2DSRB.Release();
        sm_pFillTexture2DParams.Release();

        sm_pFillTexture3DPSO.Release();
        sm_pFillTexture3DSRB.Release();
        sm_pFillTexture3DParams.Release();

        sm_pTempSRB.Release();
    }

    static RefCntAutoPtr<IBuffer> CreateSparseBuffer(Uint64 Size, BIND_FLAGS BindFlags, bool Aliasing = false, const Uint32 Stride = 4u)
    {
        auto* pDevice = TestingEnvironment::GetInstance()->GetDevice();

        BufferDesc Desc;
        Desc.Name              = "Sparse buffer";
        Desc.Size              = AlignUp(Size, Stride);
        Desc.BindFlags         = BindFlags | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS; // UAV for fill buffer, SRV to read in PS
        Desc.Usage             = USAGE_SPARSE;
        Desc.MiscFlags         = (Aliasing ? MISC_BUFFER_FLAG_SPARSE_ALIASING : MISC_BUFFER_FLAG_NONE);
        Desc.Mode              = BUFFER_MODE_STRUCTURED;
        Desc.ElementByteStride = Stride;

        RefCntAutoPtr<IBuffer> pBuffer;
        pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
        return pBuffer;
    }

    static RefCntAutoPtr<IBuffer> CreateBuffer(Uint64 Size, BIND_FLAGS BindFlags, const Uint32 Stride = 4u)
    {
        auto* pDevice = TestingEnvironment::GetInstance()->GetDevice();

        BufferDesc Desc;
        Desc.Name              = "Reference buffer";
        Desc.Size              = AlignUp(Size, Stride);
        Desc.BindFlags         = BindFlags | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS; // UAV for fill buffer, SRV to read in PS
        Desc.Usage             = USAGE_DEFAULT;
        Desc.Mode              = BUFFER_MODE_STRUCTURED;
        Desc.ElementByteStride = Stride;

        RefCntAutoPtr<IBuffer> pBuffer;
        pDevice->CreateBuffer(Desc, nullptr, &pBuffer);
        return pBuffer;
    }

    static RefCntAutoPtr<IDeviceMemory> CreateMemory(Uint32 PageSize, Uint32 NumPages, IDeviceObject* pCompatibleResource)
    {
        auto* pDevice = TestingEnvironment::GetInstance()->GetDevice();

        DeviceMemoryCreateInfo MemCI;
        MemCI.Desc.Name             = "Memory for sparse resources";
        MemCI.Desc.Type             = DEVICE_MEMORY_TYPE_SPARSE;
        MemCI.Desc.PageSize         = PageSize;
        MemCI.InitialSize           = NumPages * PageSize;
        MemCI.ppCompatibleResources = &pCompatibleResource;
        MemCI.NumResources          = pCompatibleResource == nullptr ? 0 : 1;

        RefCntAutoPtr<IDeviceMemory> pMemory;
        pDevice->CreateDeviceMemory(MemCI, &pMemory);
        if (pMemory == nullptr)
            return {};

        // Even if resize is not supported function must return 'true'
        if (!pMemory->Resize(MemCI.InitialSize))
            return {};

        VERIFY_EXPR(pMemory->GetCapacity() == NumPages * PageSize);

        return pMemory;
    }

    struct TextureAndMemory
    {
        RefCntAutoPtr<ITexture>      pTexture;
        RefCntAutoPtr<IDeviceMemory> pMemory;
    };
    static TextureAndMemory CreateSparseTextureAndMemory(const uint4& Dim, BIND_FLAGS BindFlags, Uint32 NumMemoryPages, bool Aliasing = false)
    {
        auto*      pDevice   = TestingEnvironment::GetInstance()->GetDevice();
        const auto BlockSize = pDevice->GetAdapterInfo().SparseMemory.StandardBlockSize;

        TextureDesc Desc;
        Desc.BindFlags = BindFlags | BIND_SHADER_RESOURCE; // SRV to read in PS
        if (Dim.z > 1)
        {
            VERIFY_EXPR(Dim.w <= 1);
            Desc.Type  = RESOURCE_DIM_TEX_3D;
            Desc.Depth = static_cast<Uint32>(Dim.z);
            Desc.BindFlags |= BIND_UNORDERED_ACCESS; // UAV to fill texture
        }
        else
        {
            VERIFY_EXPR(Dim.z <= 1);
            Desc.Type      = Dim.w > 1 ? RESOURCE_DIM_TEX_2D_ARRAY : RESOURCE_DIM_TEX_2D;
            Desc.ArraySize = static_cast<Uint32>(Dim.w);
            Desc.BindFlags |= BIND_RENDER_TARGET; // RTV to fill texture
        }

        Desc.Width       = static_cast<Uint32>(Dim.x);
        Desc.Height      = static_cast<Uint32>(Dim.y);
        Desc.Format      = sm_TexFormat;
        Desc.MipLevels   = 0; // full mip chain
        Desc.SampleCount = 1;
        Desc.Usage       = USAGE_SPARSE;
        Desc.MiscFlags   = (Aliasing ? MISC_TEXTURE_FLAG_SPARSE_ALIASING : MISC_TEXTURE_FLAG_NONE);

        TextureAndMemory Result;
        if (pDevice->GetDeviceInfo().IsMetalDevice())
        {
#if PLATFORM_MACOS
            Result.pMemory = CreateMemory(AlignUp(64u << 10, BlockSize), NumMemoryPages, nullptr);
            if (Result.pMemory == nullptr)
                return {};

            RefCntAutoPtr<IRenderDeviceMtl> pDeviceMtl{pDevice, IID_RenderDeviceMtl};
            pDeviceMtl->CreateSparseTexture(Desc, Result.pMemory, &Result.pTexture);
#endif
        }
        else
        {
            pDevice->CreateTexture(Desc, nullptr, &Result.pTexture);
            if (Result.pTexture == nullptr)
                return {};

            Result.pMemory = CreateMemory(BlockSize, NumMemoryPages, Result.pTexture);
        }
        return Result;
    }

    static RefCntAutoPtr<ITexture> CreateTexture(const uint4& Dim, BIND_FLAGS BindFlags)
    {
        auto* pDevice = TestingEnvironment::GetInstance()->GetDevice();

        TextureDesc Desc;
        Desc.BindFlags = BindFlags | BIND_SHADER_RESOURCE; // SRV to read in PS
        if (Dim.z > 1)
        {
            VERIFY_EXPR(Dim.w <= 1);
            Desc.Type  = RESOURCE_DIM_TEX_3D;
            Desc.Depth = static_cast<Uint32>(Dim.z);
            Desc.BindFlags |= BIND_UNORDERED_ACCESS; // UAV to fill texture
        }
        else
        {
            VERIFY_EXPR(Dim.z <= 1);
            Desc.Type      = Dim.w > 1 ? RESOURCE_DIM_TEX_2D_ARRAY : RESOURCE_DIM_TEX_2D;
            Desc.ArraySize = static_cast<Uint32>(Dim.w);
            Desc.BindFlags |= BIND_RENDER_TARGET; // RTV to fill texture
        }

        Desc.Width       = static_cast<Uint32>(Dim.x);
        Desc.Height      = static_cast<Uint32>(Dim.y);
        Desc.Format      = sm_TexFormat;
        Desc.MipLevels   = 0; // full mip chain
        Desc.SampleCount = 1;
        Desc.Usage       = USAGE_DEFAULT;

        RefCntAutoPtr<ITexture> pTexture;
        pDevice->CreateTexture(Desc, nullptr, &pTexture);
        return pTexture;
    }

    static RefCntAutoPtr<IFence> CreateFence()
    {
        auto* pDevice = TestingEnvironment::GetInstance()->GetDevice();

        if (pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D11)
            return {};

        FenceDesc Desc;
        Desc.Name = "Fence";
        Desc.Type = FENCE_TYPE_GENERAL;

        RefCntAutoPtr<IFence> pFence;
        pDevice->CreateFence(Desc, &pFence);

        return pFence;
    }

    static void FillBuffer(IDeviceContext* pContext, IBuffer* pBuffer, Uint64 Offset, Uint32 Size, Uint32 Pattern)
    {
        auto* pView = pBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
        VERIFY_EXPR(pView != nullptr);

        sm_pFillBufferSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_DstBuffer")->Set(pView);

        const auto Stride = pBuffer->GetDesc().ElementByteStride;

        struct CB
        {
            Uint32 Offset;
            Uint32 Size;
            Uint32 Pattern;
            Uint32 padding;
        };
        {
            MapHelper<CB> CBConstants(pContext, sm_pFillBufferParams, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->Offset  = StaticCast<Uint32>(Offset / Stride);
            CBConstants->Size    = Size / Stride;
            CBConstants->Pattern = Pattern;
        }

        pContext->SetPipelineState(sm_pFillBufferPSO);
        pContext->CommitShaderResources(sm_pFillBufferSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DispatchComputeAttribs CompAttrs;
        CompAttrs.ThreadGroupCountX = (Size / Stride + 63) / 64;
        CompAttrs.ThreadGroupCountY = 1;
        CompAttrs.ThreadGroupCountZ = 1;
        pContext->DispatchCompute(CompAttrs);
    }

    static void FillTextureMip(IDeviceContext* pContext, ITexture* pTexture, Uint32 MipLevel, Uint32 Slice, const float4& Color)
    {
        const auto& Desc = pTexture->GetDesc();
        const Rect  Region{0, 0, static_cast<int>(std::max(1u, Desc.Width >> MipLevel)), static_cast<int>(std::max(1u, Desc.Height >> MipLevel))};

        FillTexture(pContext, pTexture, Region, MipLevel, Slice, Color);
    }

    static void FillTexture(IDeviceContext* pContext, ITexture* pTexture, const Rect& Region, Uint32 MipLevel, Uint32 Slice, const float4& Color)
    {
        VERIFY_EXPR(pTexture->GetDesc().Is2D());

        TextureViewDesc Desc;
        Desc.ViewType        = TEXTURE_VIEW_RENDER_TARGET;
        Desc.TextureDim      = RESOURCE_DIM_TEX_2D_ARRAY;
        Desc.MostDetailedMip = MipLevel;
        Desc.NumMipLevels    = 1;
        Desc.FirstArraySlice = Slice;
        Desc.NumArraySlices  = 1;

        RefCntAutoPtr<ITextureView> pView;
        pTexture->CreateView(Desc, &pView);
        VERIFY_EXPR(pView != nullptr);

        ITextureView* pRTV = pView;

        pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        pContext->SetScissorRects(1, &Region, 0, 0);

        pContext->SetPipelineState(sm_pFillTexture2DPSO);
        pContext->CommitShaderResources(sm_pFillTexture2DSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        {
            MapHelper<float4> CBConstants(pContext, sm_pFillTexture2DParams, MAP_WRITE, MAP_FLAG_DISCARD);
            *CBConstants = Color;
        }

        DrawAttribs DrawAttrs{3, DRAW_FLAG_VERIFY_ALL};
        pContext->Draw(DrawAttrs);

        pContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

    static void ClearTexture(IDeviceContext* pContext, ITexture* pTexture)
    {
        // sparse render target must be cleared

        VERIFY_EXPR(pTexture->GetDesc().Is2D());

        const auto& TexDesc = pTexture->GetDesc();
        for (Uint32 Slice = 0; Slice < TexDesc.ArraySize; ++Slice)
        {
            for (Uint32 Mip = 0; Mip < TexDesc.MipLevels; ++Mip)
            {
                TextureViewDesc Desc;
                Desc.ViewType        = TEXTURE_VIEW_RENDER_TARGET;
                Desc.TextureDim      = RESOURCE_DIM_TEX_2D_ARRAY;
                Desc.MostDetailedMip = Mip;
                Desc.NumMipLevels    = 1;
                Desc.FirstArraySlice = Slice;
                Desc.NumArraySlices  = 1;

                RefCntAutoPtr<ITextureView> pView;
                pTexture->CreateView(Desc, &pView);
                VERIFY_EXPR(pView != nullptr);

                ITextureView* pRTV = pView;

                pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

                const float ClearColor[4] = {};
                pContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_NONE);

                pContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
            }
        }
    }

    static void FillTexture3DMip(IDeviceContext* pContext, ITexture* pTexture, Uint32 MipLevel, const float4& Color)
    {
        const auto& Desc = pTexture->GetDesc();
        const Box   Region{
            0u, std::max(1u, Desc.Width >> MipLevel),
            0u, std::max(1u, Desc.Height >> MipLevel),
            0u, std::max(1u, Desc.Depth >> MipLevel)};

        FillTexture3D(pContext, pTexture, Region, MipLevel, Color);
    }

    static void FillTexture3D(IDeviceContext* pContext, ITexture* pTexture, const Box& Region, Uint32 MipLevel, const float4& Color)
    {
        VERIFY_EXPR(pTexture->GetDesc().Is3D());

        TextureViewDesc Desc;
        Desc.ViewType        = TEXTURE_VIEW_UNORDERED_ACCESS;
        Desc.TextureDim      = RESOURCE_DIM_TEX_3D;
        Desc.MostDetailedMip = MipLevel;
        Desc.NumMipLevels    = 1;
        Desc.FirstDepthSlice = 0;
        Desc.NumDepthSlices  = 0; // all slices

        RefCntAutoPtr<ITextureView> pView;
        pTexture->CreateView(Desc, &pView);
        VERIFY_EXPR(pView);

        sm_pFillTexture3DSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_DstTexture")->Set(pView);

        struct CB
        {
            uint4  Offset;
            uint4  Size;
            float4 Color;
        };
        {
            MapHelper<CB> CBConstants(pContext, sm_pFillTexture3DParams, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->Offset = {Region.MinX, Region.MinY, Region.MinZ, 0u};
            CBConstants->Size   = {Region.Width(), Region.Height(), Region.Depth(), 0u};
            CBConstants->Color  = Color;
        }

        pContext->SetPipelineState(sm_pFillTexture3DPSO);
        pContext->CommitShaderResources(sm_pFillTexture3DSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DispatchComputeAttribs CompAttrs;
        CompAttrs.ThreadGroupCountX = (Region.Width() + 3) / 4;
        CompAttrs.ThreadGroupCountY = (Region.Height() + 3) / 4;
        CompAttrs.ThreadGroupCountZ = (Region.Depth() + 3) / 4;
        pContext->DispatchCompute(CompAttrs);
    }

    static void DrawFSQuad(IDeviceContext* pContext, IPipelineState* pPSO, IShaderResourceBinding* pSRB)
    {
        auto* pEnv       = TestingEnvironment::GetInstance();
        auto* pSwapChain = pEnv->GetSwapChain();

        pContext->SetPipelineState(pPSO);
        pContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
        pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float ClearColor[4] = {0.f, 0.f, 0.f, 1.f};
        pContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_NONE);

        DrawAttribs DrawAttrs{3, DRAW_FLAG_VERIFY_ALL};
        pContext->Draw(DrawAttrs);
    }

    static void DrawFSQuadWithBuffer(IDeviceContext* pContext, IPipelineState* pPSO, IBuffer* pBuffer)
    {
        RefCntAutoPtr<IShaderResourceBinding> pSRB;
        pPSO->CreateShaderResourceBinding(&pSRB);
        if (pSRB == nullptr)
            return;

        auto* pView = pBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        VERIFY_EXPR(pView != nullptr);

        pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Buffer")->Set(pView);

        DrawFSQuad(pContext, pPSO, pSRB);

        sm_pTempSRB = std::move(pSRB);
    }

    static void DrawFSQuadWithTexture(IDeviceContext* pContext, IPipelineState* pPSO, ITexture* pTexture)
    {
        RefCntAutoPtr<IShaderResourceBinding> pSRB;
        pPSO->CreateShaderResourceBinding(&pSRB);
        if (pSRB == nullptr)
            return;

        auto* pView = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        VERIFY_EXPR(pView);

        pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pView);

        DrawFSQuad(pContext, pPSO, pSRB);

        sm_pTempSRB = std::move(pSRB);
    }

    static void CreateGraphicsPSO(const char* Name, const String& PSSource, bool Is2DArray, RefCntAutoPtr<IPipelineState>& pPSO)
    {
        auto*       pEnv       = TestingEnvironment::GetInstance();
        auto*       pDevice    = pEnv->GetDevice();
        auto*       pSwapChain = pEnv->GetSwapChain();
        const auto& SCDesc     = pSwapChain->GetDesc();

        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        auto& PSODesc          = PSOCreateInfo.PSODesc;
        auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        PSODesc.Name = Name;

        PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

        GraphicsPipeline.NumRenderTargets                     = 1;
        GraphicsPipeline.RTVFormats[0]                        = SCDesc.ColorBufferFormat;
        GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
        GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
        GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
        GraphicsPipeline.DepthStencilDesc.DepthEnable         = False;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.UseCombinedTextureSamplers = true;

        if (pDevice->GetDeviceInfo().IsVulkanDevice())
            ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC; // glslang does not support sparse residency status

        ShaderMacroHelper Macros;
        Macros.AddShaderMacro("SCREEN_WIDTH", SCDesc.Width);
        Macros.AddShaderMacro("SCREEN_HEIGHT", SCDesc.Height);
        Macros.AddShaderMacro("TEXTURE_2D_ARRAY", Is2DArray);
        ShaderCI.Macros = Macros;

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Sparse resource test - VS";
            ShaderCI.Source          = HLSL::SparseMemoryTest_VS.c_str();

            pDevice->CreateShader(ShaderCI, &pVS);
            if (pVS == nullptr)
                return;
        }

        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Sparse resource test - PS";
            ShaderCI.Source          = PSSource.c_str();

            pDevice->CreateShader(ShaderCI, &pPS);
            if (pPS == nullptr)
                return;
        }

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;

        pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
        if (pPSO == nullptr)
            return;
    }

    // Generates reproducible color sequence
    static void RestartColorRandomizer()
    {
        sm_RndColorIndex = 0.f;
    }
    static float4 RandomColor()
    {
        float h = FastFrac(sm_RndColorIndex) / 1.35f;
        sm_RndColorIndex += 0.27f;
        float3 col = float3{std::abs(h * 6.0f - 3.0f) - 1.0f, 2.0f - std::abs(h * 6.0f - 2.0f), 2.0f - std::abs(h * 6.0f - 4.0f)};
        return float4{clamp(col, float3{}, float3{1.0f}), 1.0f};
    }
    static Uint32 RandomColorU()
    {
        return F4Color_To_RGBA8Unorm(RandomColor());
    }

    static float4 GetNullBoundTileColor()
    {
        return float4{1.0, 0.0, 1.0, 1.0};
    }

    static RefCntAutoPtr<IDeviceContext> sm_pSparseBindingCtx;

    static RefCntAutoPtr<IPipelineState>         sm_pFillBufferPSO;
    static RefCntAutoPtr<IShaderResourceBinding> sm_pFillBufferSRB;
    static RefCntAutoPtr<IBuffer>                sm_pFillBufferParams;

    static RefCntAutoPtr<IPipelineState>         sm_pFillTexture2DPSO;
    static RefCntAutoPtr<IShaderResourceBinding> sm_pFillTexture2DSRB;
    static RefCntAutoPtr<IBuffer>                sm_pFillTexture2DParams;

    static RefCntAutoPtr<IPipelineState>         sm_pFillTexture3DPSO;
    static RefCntAutoPtr<IShaderResourceBinding> sm_pFillTexture3DSRB;
    static RefCntAutoPtr<IBuffer>                sm_pFillTexture3DParams;

    static RefCntAutoPtr<IShaderResourceBinding> sm_pTempSRB;

    static const TEXTURE_FORMAT sm_TexFormat = TEX_FORMAT_RGBA8_UNORM;

    static float sm_RndColorIndex;
};
RefCntAutoPtr<IDeviceContext>         SparseMemoryTest::sm_pSparseBindingCtx;
RefCntAutoPtr<IPipelineState>         SparseMemoryTest::sm_pFillBufferPSO;
RefCntAutoPtr<IShaderResourceBinding> SparseMemoryTest::sm_pFillBufferSRB;
RefCntAutoPtr<IBuffer>                SparseMemoryTest::sm_pFillBufferParams;
RefCntAutoPtr<IPipelineState>         SparseMemoryTest::sm_pFillTexture2DPSO;
RefCntAutoPtr<IShaderResourceBinding> SparseMemoryTest::sm_pFillTexture2DSRB;
RefCntAutoPtr<IBuffer>                SparseMemoryTest::sm_pFillTexture2DParams;
RefCntAutoPtr<IPipelineState>         SparseMemoryTest::sm_pFillTexture3DPSO;
RefCntAutoPtr<IShaderResourceBinding> SparseMemoryTest::sm_pFillTexture3DSRB;
RefCntAutoPtr<IBuffer>                SparseMemoryTest::sm_pFillTexture3DParams;
float                                 SparseMemoryTest::sm_RndColorIndex;
RefCntAutoPtr<IShaderResourceBinding> SparseMemoryTest::sm_pTempSRB;


enum TestMode
{
    BeginRange = 0,
    POT_2D     = BeginRange,
    POT_2DArray,
    NonPOT_2D,
    NonPOT_2DArray,
    EndRange
};

bool TestMode_IsTexArray(Uint32 Mode)
{
    return Mode == POT_2DArray || Mode == NonPOT_2DArray;
}

const auto TestParamRange = testing::Range(int{BeginRange}, int{EndRange});

String TestIdToString(const testing::TestParamInfo<int>& info)
{
    String name;
    switch (info.param)
    {
        // clang-format off
        case POT_2D:         name = "POT_2D";          break;
        case NonPOT_2D:      name = "NonPOT_2D";       break;
        case POT_2DArray:    name += "POT_2DArray";    break;
        case NonPOT_2DArray: name += "NonPOT_2DArray"; break;
        default:             name = std::to_string(info.param); UNEXPECTED("unsupported TestId");
            // clang-format on
    }
    return name;
}

int4 TestIdToTextureDim(Uint32 TestId)
{
    switch (TestId)
    {
        // clang-format off
        case POT_2D:         return int4{256, 256, 1, 1};
        case NonPOT_2D:      return int4{253, 249, 1, 1};
        case POT_2DArray:    return int4{256, 256, 1, 2};
        case NonPOT_2DArray: return int4{248, 254, 1, 2};
        default:             return int4{};
            // clang-format on
    }
}

void CheckTextureSparseProperties(ITexture* pTexture)
{
    const auto& Desc       = pTexture->GetDesc();
    const auto& Props      = pTexture->GetSparseProperties();
    const bool  IsStdBlock = (Props.Flags & SPARSE_TEXTURE_FLAG_NONSTANDARD_BLOCK_SIZE) == 0;
    const auto& SparseMem  = TestingEnvironment::GetInstance()->GetDevice()->GetAdapterInfo().SparseMemory;

    ASSERT_GT(Props.MemorySize, 0u);
    ASSERT_GT(Props.BlockSize, 0u);
    ASSERT_TRUE(Props.MemorySize % Props.BlockSize == 0);

    if (IsStdBlock)
        ASSERT_EQ(Props.BlockSize, SparseMem.StandardBlockSize);

    ASSERT_LE(Props.FirstMipInTail, Desc.MipLevels);
    ASSERT_LT(Props.MipTailOffset, Props.MemorySize);
    ASSERT_TRUE(Props.MipTailOffset % Props.BlockSize == 0);

    // Props.MipTailSize can be zero
    ASSERT_TRUE(Props.MipTailSize % Props.BlockSize == 0);

    if (Desc.Type == RESOURCE_DIM_TEX_3D || Desc.ArraySize == 1)
    {
        ASSERT_GE(Props.MemorySize, Props.MipTailOffset + Props.MipTailSize);
    }
    else if (Props.MipTailStride != 0) // zero in Metal
    {
        ASSERT_EQ(Props.MipTailStride * Desc.ArraySize, Props.MemorySize);
        ASSERT_GE(Props.MipTailStride, Props.MipTailOffset + Props.MipTailSize);
    }

    if (Desc.Type == RESOURCE_DIM_TEX_3D)
    {
        ASSERT_GT(Props.TileSize[0], 1u);
        ASSERT_GT(Props.TileSize[1], 1u);
        ASSERT_GT(Props.TileSize[2], 1u);

        if (IsStdBlock)
        {
            ASSERT_TRUE((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_STANDARD_3D_BLOCK_SHAPE) != 0);
            ASSERT_EQ(Props.TileSize[0], 32u);
            ASSERT_EQ(Props.TileSize[1], 32u);
            ASSERT_EQ(Props.TileSize[2], 16u);
        }
    }
    else
    {
        ASSERT_GT(Props.TileSize[0], 1u);
        ASSERT_GT(Props.TileSize[1], 1u);
        ASSERT_EQ(Props.TileSize[2], 1u);

        if (IsStdBlock)
        {
            ASSERT_TRUE((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_STANDARD_2D_BLOCK_SHAPE) != 0);
            ASSERT_EQ(Props.TileSize[0], 128u);
            ASSERT_EQ(Props.TileSize[1], 128u);
            ASSERT_EQ(Props.TileSize[2], 1u);
        }
    }
}


TEST_F(SparseMemoryTest, SparseBuffer)
{
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if (!sm_pSparseBindingCtx)
    {
        GTEST_SKIP() << "Sparse binding queue is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_BUFFER) == 0)
    {
        GTEST_SKIP() << "Sparse buffer is not supported by this device";
    }

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<IPipelineState> pPSO;
    CreateGraphicsPSO("Sparse buffer test", HLSL::SparseBuffer_PS, false, pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto BlockSize = 64u << 10;
    const auto BuffSize  = BlockSize * 4;

    const auto Fill = [&](IBuffer* pBuffer) //
    {
        RestartColorRandomizer();
        FillBuffer(pContext, pBuffer, BlockSize * 0, BlockSize, RandomColorU());
        FillBuffer(pContext, pBuffer, BlockSize * 1, BlockSize, RandomColorU());
        FillBuffer(pContext, pBuffer, BlockSize * 2, BlockSize, RandomColorU());
        FillBuffer(pContext, pBuffer, BlockSize * 3, BlockSize, RandomColorU());
    };

    // Draw reference
    {
        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

        auto pBuffer = CreateBuffer(BuffSize, BIND_NONE);
        ASSERT_NE(pBuffer, nullptr);

        Fill(pBuffer);
        DrawFSQuadWithBuffer(pContext, pPSO, pBuffer);

        auto pRT = pSwapChain->GetCurrentBackBufferRTV()->GetTexture();

        // Transition to CopySrc state to use in TakeSnapshot()
        StateTransitionDesc Barrier{pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);

        pContext->Flush();
        pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

        pTestingSwapChain->TakeSnapshot(pRT);
    }

    auto pBuffer = CreateSparseBuffer(BuffSize, BIND_NONE);
    ASSERT_NE(pBuffer, nullptr);
    ASSERT_NE(pBuffer->GetNativeHandle(), 0);

    const auto MemBlockSize = BlockSize;
    auto       pMemory      = CreateMemory(MemBlockSize * 2, 4, pBuffer);
    ASSERT_NE(pMemory, nullptr);

    auto pFence = CreateFence();

    // bind sparse
    {
        const SparseBufferMemoryBindRange BindRanges[] = {
            {BlockSize * 0, MemBlockSize * 0, BlockSize, pMemory},
            {BlockSize * 1, MemBlockSize * 2, BlockSize, pMemory},
            {BlockSize * 2, MemBlockSize * 3, BlockSize, pMemory},
            {BlockSize * 3, MemBlockSize * 6, BlockSize, pMemory} //
        };

        SparseBufferMemoryBind SparseBuffBind;
        SparseBuffBind.pBuffer   = pBuffer;
        SparseBuffBind.NumRanges = _countof(BindRanges);
        SparseBuffBind.pRanges   = BindRanges;

        BindSparseMemoryAttribs BindSparseAttrs;
        BindSparseAttrs.NumBufferBinds = 1;
        BindSparseAttrs.pBufferBinds   = &SparseBuffBind;

        IFence*      SignalFence = pFence;
        const Uint64 SignalValue = 1;

        if (SignalFence)
        {
            BindSparseAttrs.ppSignalFences     = &SignalFence;
            BindSparseAttrs.pSignalFenceValues = &SignalValue;
            BindSparseAttrs.NumSignalFences    = 1;
        }

        sm_pSparseBindingCtx->BindSparseMemory(BindSparseAttrs);

        if (SignalFence)
            pContext->DeviceWaitForFence(SignalFence, SignalValue);

        Fill(pBuffer);
    }

    DrawFSQuadWithBuffer(pContext, pPSO, pBuffer);

    pSwapChain->Present();
}


TEST_F(SparseMemoryTest, SparseResidentBuffer)
{
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if (!sm_pSparseBindingCtx)
    {
        GTEST_SKIP() << "Sparse binding queue is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_BUFFER) == 0)
    {
        GTEST_SKIP() << "Sparse buffer is not supported by this device";
    }
    // Without this capability read access will return undefined values for unbound ranges and tast may fail
    //if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_NON_RESIDENT_STRICT) == 0)
    //    GTEST_SKIP() << "SPARSE_MEMORY_CAP_FLAG_NON_RESIDENT_STRICT is not supported by this device";

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<IPipelineState> pPSO;
    CreateGraphicsPSO("Sparse residency buffer test", HLSL::SparseBuffer_PS, false, pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto BlockSize = 64u << 10;
    const auto BuffSize  = BlockSize * 8;

    const auto Fill = [&](IBuffer* pBuffer) //
    {
        RestartColorRandomizer();
        FillBuffer(pContext, pBuffer, BlockSize * 0, BlockSize, RandomColorU());
        FillBuffer(pContext, pBuffer, BlockSize * 2, BlockSize, RandomColorU());
        FillBuffer(pContext, pBuffer, BlockSize * 3, BlockSize, RandomColorU());
        FillBuffer(pContext, pBuffer, BlockSize * 6, BlockSize, RandomColorU());

        if (pBuffer->GetDesc().Usage != USAGE_SPARSE)
        {
            FillBuffer(pContext, pBuffer, BlockSize * 1, BlockSize, 0);
            FillBuffer(pContext, pBuffer, BlockSize * 4, BlockSize, 0);
            FillBuffer(pContext, pBuffer, BlockSize * 5, BlockSize, 0);
            FillBuffer(pContext, pBuffer, BlockSize * 7, BlockSize, 0);
        }
    };

    // Draw reference
    {
        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

        auto pBuffer = CreateBuffer(BuffSize, BIND_NONE);
        ASSERT_NE(pBuffer, nullptr);

        Fill(pBuffer);
        DrawFSQuadWithBuffer(pContext, pPSO, pBuffer);

        auto pRT = pSwapChain->GetCurrentBackBufferRTV()->GetTexture();

        // Transition to CopySrc state to use in TakeSnapshot()
        StateTransitionDesc Barrier{pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);

        pContext->Flush();
        pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

        pTestingSwapChain->TakeSnapshot(pRT);
    }

    auto pBuffer = CreateSparseBuffer(BuffSize, BIND_NONE);
    ASSERT_NE(pBuffer, nullptr);
    ASSERT_NE(pBuffer->GetNativeHandle(), 0);

    const auto MemBlockSize = BlockSize;
    auto       pMemory      = CreateMemory(MemBlockSize * 2, 4, pBuffer);
    ASSERT_NE(pMemory, nullptr);

    auto pFence = CreateFence();

    // bind sparse
    {
        const SparseBufferMemoryBindRange BindRanges[] = {
            // clang-format off
            {BlockSize * 0, MemBlockSize * 0, BlockSize, pMemory},
            //{BlockSize * 1,                0, BlockSize, nullptr}, // same as keep range unbounded // AZ TODO: hungs on NVidia
            {BlockSize * 2, MemBlockSize * 2, BlockSize, pMemory},
            {BlockSize * 3, MemBlockSize * 3, BlockSize, pMemory},
            {BlockSize * 6, MemBlockSize * 6, BlockSize, pMemory}
            // clang-format on
        };

        SparseBufferMemoryBind SparseBuffBind;
        SparseBuffBind.pBuffer   = pBuffer;
        SparseBuffBind.NumRanges = _countof(BindRanges);
        SparseBuffBind.pRanges   = BindRanges;

        BindSparseMemoryAttribs BindSparseAttrs;
        BindSparseAttrs.NumBufferBinds = 1;
        BindSparseAttrs.pBufferBinds   = &SparseBuffBind;

        IFence*      SignalFence = pFence;
        const Uint64 SignalValue = 1;

        if (SignalFence)
        {
            BindSparseAttrs.ppSignalFences     = &SignalFence;
            BindSparseAttrs.pSignalFenceValues = &SignalValue;
            BindSparseAttrs.NumSignalFences    = 1;
        }
        sm_pSparseBindingCtx->BindSparseMemory(BindSparseAttrs);

        if (SignalFence)
            pContext->DeviceWaitForFence(SignalFence, SignalValue);

        Fill(pBuffer);
    }

    DrawFSQuadWithBuffer(pContext, pPSO, pBuffer);

    pSwapChain->Present();
}


TEST_F(SparseMemoryTest, SparseResidentAliasedBuffer)
{
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if (!sm_pSparseBindingCtx)
    {
        GTEST_SKIP() << "Sparse binding queue is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_BUFFER) == 0)
    {
        GTEST_SKIP() << "Sparse buffer is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_ALIASED) == 0)
    {
        GTEST_SKIP() << "Sparse aliased resources is not supported by this device";
    }

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<IPipelineState> pPSO;
    CreateGraphicsPSO("Sparse residency aliased buffer test", HLSL::SparseBuffer_PS, false, pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto BlockSize = 64u << 10;
    const auto BuffSize  = BlockSize * 8;

    const auto Fill = [&](IBuffer* pBuffer) //
    {
        RestartColorRandomizer();
        const auto col = RandomColorU();
        FillBuffer(pContext, pBuffer, BlockSize * 2, BlockSize, col);
        FillBuffer(pContext, pBuffer, BlockSize * 1, BlockSize, RandomColorU());
        FillBuffer(pContext, pBuffer, BlockSize * 3, BlockSize, RandomColorU());
        FillBuffer(pContext, pBuffer, BlockSize * 5, BlockSize, RandomColorU());

        if (pBuffer->GetDesc().Usage != USAGE_SPARSE)
        {
            FillBuffer(pContext, pBuffer, BlockSize * 0, BlockSize, col);
            FillBuffer(pContext, pBuffer, BlockSize * 4, BlockSize, 0);
            FillBuffer(pContext, pBuffer, BlockSize * 6, BlockSize, 0);
            FillBuffer(pContext, pBuffer, BlockSize * 7, BlockSize, 0);
        }
    };

    // Draw reference
    {
        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

        auto pBuffer = CreateBuffer(BuffSize, BIND_NONE);
        ASSERT_NE(pBuffer, nullptr);

        Fill(pBuffer);
        DrawFSQuadWithBuffer(pContext, pPSO, pBuffer);

        auto pRT = pSwapChain->GetCurrentBackBufferRTV()->GetTexture();

        // Transition to CopySrc state to use in TakeSnapshot()
        StateTransitionDesc Barrier{pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);

        pContext->Flush();
        pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

        pTestingSwapChain->TakeSnapshot(pRT);
    }

    auto pBuffer = CreateSparseBuffer(BuffSize, BIND_NONE);
    ASSERT_NE(pBuffer, nullptr);
    ASSERT_NE(pBuffer->GetNativeHandle(), 0);

    const auto MemBlockSize = BlockSize;
    auto       pMemory      = CreateMemory(MemBlockSize * 2, 4, pBuffer);
    ASSERT_NE(pMemory, nullptr);

    auto pFence = CreateFence();

    // bind sparse
    {
        const SparseBufferMemoryBindRange BindRanges[] = {
            {BlockSize * 0, MemBlockSize * 0, BlockSize, pMemory},
            {BlockSize * 1, MemBlockSize * 2, BlockSize, pMemory},
            {BlockSize * 2, MemBlockSize * 0, BlockSize, pMemory}, // reuse 1st memory block
            {BlockSize * 3, MemBlockSize * 1, BlockSize, pMemory},
            {BlockSize * 5, MemBlockSize * 6, BlockSize, pMemory} //
        };

        SparseBufferMemoryBind SparseBuffBind;
        SparseBuffBind.pBuffer   = pBuffer;
        SparseBuffBind.NumRanges = _countof(BindRanges);
        SparseBuffBind.pRanges   = BindRanges;

        BindSparseMemoryAttribs BindSparseAttrs;
        BindSparseAttrs.NumBufferBinds = 1;
        BindSparseAttrs.pBufferBinds   = &SparseBuffBind;

        IFence*      SignalFence = pFence;
        const Uint64 SignalValue = 1;

        if (SignalFence)
        {
            BindSparseAttrs.ppSignalFences     = &SignalFence;
            BindSparseAttrs.pSignalFenceValues = &SignalValue;
            BindSparseAttrs.NumSignalFences    = 1;
        }
        sm_pSparseBindingCtx->BindSparseMemory(BindSparseAttrs);

        if (SignalFence)
            pContext->DeviceWaitForFence(SignalFence, SignalValue);

        Fill(pBuffer);
    }

    DrawFSQuadWithBuffer(pContext, pPSO, pBuffer);

    pSwapChain->Present();
}


TEST_P(SparseMemoryTest, SparseTexture)
{
    Uint32      TestId    = GetParam();
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if (!sm_pSparseBindingCtx)
    {
        GTEST_SKIP() << "Sparse binding queue is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_2D) == 0)
    {
        GTEST_SKIP() << "Sparse texture 2D is not supported by this device";
    }
    if (TestMode_IsTexArray(TestId) && (SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
    {
        GTEST_SKIP() << "Sparse texture 2D array with mipmap tail is not supported by this device";
    }

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    const auto                    TexSize = TestIdToTextureDim(TestId);
    RefCntAutoPtr<IPipelineState> pPSO;
    CreateGraphicsPSO("Sparse texture test", HLSL::SparseTexture_PS, TexSize.w > 1, pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto Fill = [&](ITexture* pTexture) //
    {
        RestartColorRandomizer();
        const auto& TexDesc = pTexture->GetDesc();
        for (Uint32 Slice = 0; Slice < TexDesc.ArraySize; ++Slice)
        {
            // clang-format off
            FillTexture(pContext, pTexture, Rect{  0,   0,       128,       128}, 0, Slice, RandomColor());
            FillTexture(pContext, pTexture, Rect{128,   0, TexSize.x,       128}, 0, Slice, RandomColor());
            FillTexture(pContext, pTexture, Rect{  0, 128,       128, TexSize.y}, 0, Slice, RandomColor());
            FillTexture(pContext, pTexture, Rect{128, 128, TexSize.x, TexSize.y}, 0, Slice, RandomColor());
            // clang-format on

            for (Uint32 Mip = 1; Mip < TexDesc.MipLevels; ++Mip)
                FillTextureMip(pContext, pTexture, Mip, Slice, RandomColor());
        }
    };

    // Draw reference
    {
        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

        auto pRefTexture = CreateTexture(TexSize.Recast<Uint32>(), BIND_NONE);
        ASSERT_NE(pRefTexture, nullptr);

        Fill(pRefTexture);
        DrawFSQuadWithTexture(pContext, pPSO, pRefTexture);

        auto pRT = pSwapChain->GetCurrentBackBufferRTV()->GetTexture();

        // Transition to CopySrc state to use in TakeSnapshot()
        StateTransitionDesc Barrier{pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);

        pContext->Flush();
        pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

        pTestingSwapChain->TakeSnapshot(pRT);
    }

    const auto BlockSize = SparseMem.StandardBlockSize;

    auto TexAndMem = CreateSparseTextureAndMemory(TexSize.Recast<Uint32>(), BIND_NONE, 14 * TexSize.w);
    auto pTexture  = TexAndMem.pTexture;
    ASSERT_NE(pTexture, nullptr);
    ASSERT_NE(pTexture->GetNativeHandle(), 0);
    auto pMemory = TexAndMem.pMemory;
    ASSERT_NE(pMemory, nullptr);

    const auto& TexDesc        = pTexture->GetDesc();
    const auto& TexSparseProps = pTexture->GetSparseProperties();
    CheckTextureSparseProperties(pTexture);
    ASSERT_LE(TexSparseProps.MemorySize, pMemory->GetCapacity());

    auto pFence = CreateFence();

    // bind sparse
    {
        std::vector<SparseTextureMemoryBindRange> BindRanges;

        Uint64 MemOffset = 0;
        for (Uint32 Slice = 0; Slice < TexDesc.ArraySize; ++Slice)
        {
            for (Uint32 Mip = 0; Mip < TexSparseProps.FirstMipInTail; ++Mip)
            {
                const auto Width  = std::max(1u, TexDesc.Width >> Mip);
                const auto Height = std::max(1u, TexDesc.Height >> Mip);
                for (Uint32 y = 0; y < Height; y += TexSparseProps.TileSize[1])
                {
                    for (Uint32 x = 0; x < Width; x += TexSparseProps.TileSize[0])
                    {
                        BindRanges.emplace_back();
                        auto& Range        = BindRanges.back();
                        Range.MipLevel     = Mip;
                        Range.ArraySlice   = Slice;
                        Range.Region.MinX  = x;
                        Range.Region.MaxX  = std::min(Width, x + TexSparseProps.TileSize[0]);
                        Range.Region.MinY  = y;
                        Range.Region.MaxY  = std::min(Height, y + TexSparseProps.TileSize[1]);
                        Range.Region.MinZ  = 0;
                        Range.Region.MaxZ  = 1;
                        Range.MemoryOffset = MemOffset;
                        Range.MemorySize   = BlockSize;
                        Range.pMemory      = pMemory;
                        MemOffset += Range.MemorySize;
                    }
                }
            }

            // Mip tail
            if (Slice == 0 || (TexSparseProps.Flags & SPARSE_TEXTURE_FLAG_SINGLE_MIPTAIL) == 0)
            {
                const bool IsMetal = pDevice->GetDeviceInfo().IsMetalDevice();
                for (Uint64 OffsetInMipTail = 0; OffsetInMipTail < TexSparseProps.MipTailSize;)
                {
                    BindRanges.emplace_back();
                    auto& Range           = BindRanges.back();
                    Range.MipLevel        = TexSparseProps.FirstMipInTail;
                    Range.ArraySlice      = Slice;
                    Range.OffsetInMipTail = OffsetInMipTail;
                    Range.MemoryOffset    = MemOffset;
                    Range.MemorySize      = IsMetal ? TexSparseProps.MipTailSize : BlockSize;
                    Range.pMemory         = pMemory;
                    MemOffset += Range.MemorySize;
                    OffsetInMipTail += Range.MemorySize;
                }
            }
        }
        VERIFY_EXPR(MemOffset <= pMemory->GetCapacity());

        SparseTextureMemoryBind SparseTexBind;
        SparseTexBind.pTexture  = pTexture;
        SparseTexBind.NumRanges = static_cast<Uint32>(BindRanges.size());
        SparseTexBind.pRanges   = BindRanges.data();

        BindSparseMemoryAttribs BindSparseAttrs;
        BindSparseAttrs.NumTextureBinds = 1;
        BindSparseAttrs.pTextureBinds   = &SparseTexBind;

        IFence*      SignalFence = pFence;
        const Uint64 SignalValue = 1;

        if (SignalFence)
        {
            BindSparseAttrs.ppSignalFences     = &SignalFence;
            BindSparseAttrs.pSignalFenceValues = &SignalValue;
            BindSparseAttrs.NumSignalFences    = 1;
        }
        sm_pSparseBindingCtx->BindSparseMemory(BindSparseAttrs);

        if (SignalFence)
            pContext->DeviceWaitForFence(SignalFence, SignalValue);

        ClearTexture(pContext, pTexture);
        Fill(pTexture);
    }

    DrawFSQuadWithTexture(pContext, pPSO, pTexture);

    pSwapChain->Present();
}


TEST_P(SparseMemoryTest, SparseResidencyTexture)
{
    Uint32      TestId    = GetParam();
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if (!sm_pSparseBindingCtx)
    {
        GTEST_SKIP() << "Sparse binding queue is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_2D) == 0)
    {
        GTEST_SKIP() << "Sparse texture 2D is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_SHADER_RESOURCE_RESIDENCY) == 0)
    {
        GTEST_SKIP() << "Shader resource residency is not supported by this device";
    }
    if (TestMode_IsTexArray(TestId) && (SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
    {
        GTEST_SKIP() << "Sparse texture 2D array with mipmap tail is not supported by this device";
    }

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    const auto                    TexSize = TestIdToTextureDim(TestId);
    RefCntAutoPtr<IPipelineState> pPSO;
    CreateGraphicsPSO("Sparse resident texture test", HLSL::SparseTextureResidency_PS, TexSize.w > 1, pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto Fill = [&](ITexture* pTexture) {
        RestartColorRandomizer();
        const auto& TexDesc = pTexture->GetDesc();
        for (Uint32 Slice = 0; Slice < TexDesc.ArraySize; ++Slice)
        {
            // clang-format off
            FillTexture(pContext, pTexture, Rect{  0,   0,       128,       128}, 0, Slice, RandomColor());
            FillTexture(pContext, pTexture, Rect{128,   0, TexSize.x,       128}, 0, Slice, RandomColor()); // -|-- null bound
            FillTexture(pContext, pTexture, Rect{  0, 128,       128, TexSize.y}, 0, Slice, RandomColor()); // -|
            FillTexture(pContext, pTexture, Rect{128, 128, TexSize.x, TexSize.y}, 0, Slice, RandomColor());
            // clang-format on

            for (Uint32 Mip = 1; Mip < TexDesc.MipLevels; ++Mip)
                FillTextureMip(pContext, pTexture, Mip, Slice, RandomColor());

            if (TexDesc.Usage != USAGE_SPARSE)
            {
                // clang-format off
                FillTexture(pContext, pTexture, Rect{128,   0, TexSize.x,       128}, 0, Slice, GetNullBoundTileColor());
                FillTexture(pContext, pTexture, Rect{  0, 128,       128, TexSize.y}, 0, Slice, GetNullBoundTileColor());
                // clang-format on
            }
        }
    };

    // Draw reference
    {
        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

        auto pRefTexture = CreateTexture(TexSize.Recast<Uint32>(), BIND_NONE);
        ASSERT_NE(pRefTexture, nullptr);

        Fill(pRefTexture);
        DrawFSQuadWithTexture(pContext, pPSO, pRefTexture);

        auto pRT = pSwapChain->GetCurrentBackBufferRTV()->GetTexture();

        // Transition to CopySrc state to use in TakeSnapshot()
        StateTransitionDesc Barrier{pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);

        pContext->Flush();
        pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

        pTestingSwapChain->TakeSnapshot(pRT);
    }

    const auto BlockSize = SparseMem.StandardBlockSize;

    auto TexAndMem = CreateSparseTextureAndMemory(TexSize.Recast<Uint32>(), BIND_NONE, 12 * TexSize.w);
    auto pTexture  = TexAndMem.pTexture;
    ASSERT_NE(pTexture, nullptr);
    ASSERT_NE(pTexture->GetNativeHandle(), 0);
    auto pMemory = TexAndMem.pMemory;
    ASSERT_NE(pMemory, nullptr);

    const auto& TexDesc        = pTexture->GetDesc();
    const auto& TexSparseProps = pTexture->GetSparseProperties();
    CheckTextureSparseProperties(pTexture);
    ASSERT_LE(TexSparseProps.MemorySize, pMemory->GetCapacity());

    auto pFence = CreateFence();

    // bind sparse
    {
        std::vector<SparseTextureMemoryBindRange> BindRanges;

        Uint64 MemOffset = 0;
        for (Uint32 Slice = 0; Slice < TexDesc.ArraySize; ++Slice)
        {
            for (Uint32 Mip = 0, Idx = 0; Mip < TexSparseProps.FirstMipInTail; ++Mip)
            {
                const Uint32 Width  = std::max(1u, TexDesc.Width >> Mip);
                const Uint32 Height = std::max(1u, TexDesc.Height >> Mip);
                for (Uint32 TileY = 0; TileY < Height; TileY += TexSparseProps.TileSize[1])
                {
                    for (Uint32 TileX = 0; TileX < Width; TileX += TexSparseProps.TileSize[0])
                    {
                        BindRanges.emplace_back();
                        auto& Range       = BindRanges.back();
                        Range.Region.MinX = TileX;
                        Range.Region.MaxX = TileX + TexSparseProps.TileSize[0];
                        Range.Region.MinY = TileY;
                        Range.Region.MaxY = TileY + TexSparseProps.TileSize[1];
                        Range.Region.MinZ = 0;
                        Range.Region.MaxZ = 1;
                        Range.MipLevel    = Mip;
                        Range.ArraySlice  = Slice;
                        Range.MemorySize  = BlockSize;

                        if ((++Idx & 2) == 0 || Mip > 0)
                        {
                            Range.MemoryOffset = MemOffset;
                            Range.pMemory      = pMemory;
                            MemOffset += Range.MemorySize;
                        }
                    }
                }
            }

            // Mip tail
            if (Slice == 0 || (TexSparseProps.Flags & SPARSE_TEXTURE_FLAG_SINGLE_MIPTAIL) == 0)
            {
                const bool IsMetal = pDevice->GetDeviceInfo().IsMetalDevice();
                for (Uint64 OffsetInMipTail = 0; OffsetInMipTail < TexSparseProps.MipTailSize;)
                {
                    BindRanges.emplace_back();
                    auto& Range           = BindRanges.back();
                    Range.MipLevel        = TexSparseProps.FirstMipInTail;
                    Range.ArraySlice      = Slice;
                    Range.OffsetInMipTail = OffsetInMipTail;
                    Range.MemoryOffset    = MemOffset;
                    Range.MemorySize      = IsMetal ? TexSparseProps.MipTailSize : BlockSize;
                    Range.pMemory         = pMemory;
                    MemOffset += Range.MemorySize;
                    OffsetInMipTail += Range.MemorySize;
                }
            }
        }
        VERIFY_EXPR(MemOffset <= pMemory->GetCapacity());

        SparseTextureMemoryBind SparseTexBind;
        SparseTexBind.pTexture  = pTexture;
        SparseTexBind.NumRanges = static_cast<Uint32>(BindRanges.size());
        SparseTexBind.pRanges   = BindRanges.data();

        BindSparseMemoryAttribs BindSparseAttrs;
        BindSparseAttrs.NumTextureBinds = 1;
        BindSparseAttrs.pTextureBinds   = &SparseTexBind;

        IFence*      SignalFence = pFence;
        const Uint64 SignalValue = 1;

        if (SignalFence)
        {
            BindSparseAttrs.ppSignalFences     = &SignalFence;
            BindSparseAttrs.pSignalFenceValues = &SignalValue;
            BindSparseAttrs.NumSignalFences    = 1;
        }
        sm_pSparseBindingCtx->BindSparseMemory(BindSparseAttrs);

        if (SignalFence)
            pContext->DeviceWaitForFence(SignalFence, SignalValue);

        ClearTexture(pContext, pTexture);
        Fill(pTexture);
    }

    DrawFSQuadWithTexture(pContext, pPSO, pTexture);

    pSwapChain->Present();
}


TEST_P(SparseMemoryTest, SparseResidencyAliasedTexture)
{
    Uint32      TestId    = GetParam();
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if (!sm_pSparseBindingCtx)
    {
        GTEST_SKIP() << "Sparse binding queue is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_2D) == 0)
    {
        GTEST_SKIP() << "Sparse texture 2D is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_ALIASED) == 0)
    {
        GTEST_SKIP() << "Sparse aliased resources is not supported by this device";
    }
    if (TestMode_IsTexArray(TestId) && (SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
    {
        GTEST_SKIP() << "Sparse texture 2D array with mipmap tail is not supported by this device";
    }

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    const auto                    TexSize = TestIdToTextureDim(TestId);
    RefCntAutoPtr<IPipelineState> pPSO;
    CreateGraphicsPSO("Sparse resident aliased texture test", HLSL::SparseTexture_PS, TexSize.w > 1, pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto Fill = [&](ITexture* pTexture) //
    {
        RestartColorRandomizer();
        const auto& TexDesc = pTexture->GetDesc();
        for (Uint32 Slice = 0; Slice < TexDesc.ArraySize; ++Slice)
        {
            const auto Col0 = RandomColor();
            const auto Col1 = RandomColor();

            // clang-format off
            FillTexture(pContext, pTexture, Rect{  0,   0,       128,       128}, 0, Slice, Col0);
            FillTexture(pContext, pTexture, Rect{128,   0, TexSize.x,       128}, 0, Slice, Col1);
          //FillTexture(pContext, pTexture, Rect{  0, 128,       128, TexSize.y}, 0, Slice, Col0); // -|
          //FillTexture(pContext, pTexture, Rect{128, 128, TexSize.x, TexSize.y}, 0, Slice, Col1); // -|-- aliased with 1
            // clang-format on

            if (TexDesc.Usage != USAGE_SPARSE)
            {
                // clang-format off
                FillTexture(pContext, pTexture, Rect{  0, 128,       128, TexSize.y}, 0, Slice, Col0); // -|
                FillTexture(pContext, pTexture, Rect{128, 128, TexSize.x, TexSize.y}, 0, Slice, Col1); // -|-- aliased with 1
                // clang-format on
            }

            for (Uint32 Mip = 1; Mip < TexDesc.MipLevels; ++Mip)
                FillTextureMip(pContext, pTexture, Mip, Slice, RandomColor());
        }
    };

    // Draw reference
    {
        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

        auto pRefTexture = CreateTexture(TexSize.Recast<Uint32>(), BIND_NONE);
        ASSERT_NE(pRefTexture, nullptr);

        Fill(pRefTexture);
        DrawFSQuadWithTexture(pContext, pPSO, pRefTexture);

        auto pRT = pSwapChain->GetCurrentBackBufferRTV()->GetTexture();

        // Transition to CopySrc state to use in TakeSnapshot()
        StateTransitionDesc Barrier{pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);

        pContext->Flush();
        pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

        pTestingSwapChain->TakeSnapshot(pRT);
    }

    const auto BlockSize = SparseMem.StandardBlockSize;

    auto TexAndMem = CreateSparseTextureAndMemory(TexSize.Recast<Uint32>(), BIND_NONE, 12 * TexSize.w, /*Aliasing*/ true);
    auto pTexture  = TexAndMem.pTexture;
    ASSERT_NE(pTexture, nullptr);
    ASSERT_NE(pTexture->GetNativeHandle(), 0);
    auto pMemory = TexAndMem.pMemory;
    ASSERT_NE(pMemory, nullptr);

    const auto& TexDesc        = pTexture->GetDesc();
    const auto& TexSparseProps = pTexture->GetSparseProperties();
    CheckTextureSparseProperties(pTexture);
    ASSERT_LE(TexSparseProps.MemorySize, pMemory->GetCapacity());

    auto pFence = CreateFence();

    // bind sparse
    {
        std::vector<SparseTextureMemoryBindRange> BindRanges;

        // Mip tail - must not alias with other tiles
        Uint64       InitialOffset = 0;
        const Uint32 MipTailSlices = (TexSparseProps.Flags & SPARSE_TEXTURE_FLAG_SINGLE_MIPTAIL) != 0 ? 1 : TexDesc.ArraySize;
        const bool   IsMetal       = pDevice->GetDeviceInfo().IsMetalDevice();
        for (Uint32 Slice = 0; Slice < MipTailSlices; ++Slice)
        {
            for (Uint64 OffsetInMipTail = 0; OffsetInMipTail < TexSparseProps.MipTailSize;)
            {
                BindRanges.emplace_back();
                auto& Range           = BindRanges.back();
                Range.MipLevel        = TexSparseProps.FirstMipInTail;
                Range.ArraySlice      = Slice;
                Range.OffsetInMipTail = OffsetInMipTail;
                Range.MemoryOffset    = InitialOffset;
                Range.MemorySize      = IsMetal ? TexSparseProps.MipTailSize : BlockSize;
                Range.pMemory         = pMemory;
                InitialOffset += Range.MemorySize;
                OffsetInMipTail += Range.MemorySize;
            }
        }

        // tiles may alias
        for (Uint32 Slice = 0; Slice < TexDesc.ArraySize; ++Slice)
        {
            Uint64 MemOffset = InitialOffset;
            for (Uint32 Mip = 0, Idx = 0; Mip < TexSparseProps.FirstMipInTail; ++Mip)
            {
                const Uint32 Width  = std::max(1u, TexDesc.Width >> Mip);
                const Uint32 Height = std::max(1u, TexDesc.Height >> Mip);
                for (Uint32 y = 0; y < Height; y += TexSparseProps.TileSize[1])
                {
                    for (Uint32 x = 0; x < Width; x += TexSparseProps.TileSize[0])
                    {
                        if (++Idx > 2 && Mip == 0)
                        {
                            Idx       = 0;
                            MemOffset = InitialOffset;
                        }

                        BindRanges.emplace_back();
                        auto& Range        = BindRanges.back();
                        Range.Region.MinX  = x;
                        Range.Region.MaxX  = x + TexSparseProps.TileSize[0];
                        Range.Region.MinY  = y;
                        Range.Region.MaxY  = y + TexSparseProps.TileSize[1];
                        Range.Region.MinZ  = 0;
                        Range.Region.MaxZ  = 1;
                        Range.MipLevel     = Mip;
                        Range.ArraySlice   = Slice;
                        Range.MemoryOffset = MemOffset;
                        Range.MemorySize   = BlockSize;
                        Range.pMemory      = pMemory;

                        MemOffset += Range.MemorySize;
                        VERIFY_EXPR(MemOffset <= pMemory->GetCapacity());
                    }
                }
            }
            InitialOffset = MemOffset;
        }

        SparseTextureMemoryBind SparseTexBind;
        SparseTexBind.pTexture  = pTexture;
        SparseTexBind.NumRanges = static_cast<Uint32>(BindRanges.size());
        SparseTexBind.pRanges   = BindRanges.data();

        BindSparseMemoryAttribs BindSparseAttrs;
        BindSparseAttrs.NumTextureBinds = 1;
        BindSparseAttrs.pTextureBinds   = &SparseTexBind;

        IFence*      SignalFence = pFence;
        const Uint64 SignalValue = 1;

        if (SignalFence)
        {
            BindSparseAttrs.ppSignalFences     = &SignalFence;
            BindSparseAttrs.pSignalFenceValues = &SignalValue;
            BindSparseAttrs.NumSignalFences    = 1;
        }
        sm_pSparseBindingCtx->BindSparseMemory(BindSparseAttrs);

        if (SignalFence)
            pContext->DeviceWaitForFence(SignalFence, SignalValue);

        ClearTexture(pContext, pTexture);
        Fill(pTexture);
    }

    DrawFSQuadWithTexture(pContext, pPSO, pTexture);

    pSwapChain->Present();
}

INSTANTIATE_TEST_SUITE_P(Sparse, SparseMemoryTest, TestParamRange, TestIdToString);


TEST_F(SparseMemoryTest, SparseTexture3D)
{
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if (!sm_pSparseBindingCtx)
    {
        GTEST_SKIP() << "Sparse binding queue is not supported by this device";
    }
    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_3D) == 0)
    {
        GTEST_SKIP() << "Sparse texture 3D is not supported by this device";
    }
    if (pDevice->GetDeviceInfo().IsMetalDevice())
        GTEST_SKIP() << "UAV sparse texture is not supported in Metal";

    TestingEnvironment::ScopedReset EnvironmentAutoReset;

    auto* pSwapChain = pEnv->GetSwapChain();
    auto* pContext   = pEnv->GetDeviceContext();

    RefCntAutoPtr<IPipelineState> pPSO;
    CreateGraphicsPSO("Sparse texture 3d test", HLSL::SparseTexture3D_PS, false, pPSO);
    ASSERT_NE(pPSO, nullptr);

    const auto TexSize = uint4{64, 64, 15, 1};

    const auto Fill = [&](ITexture* pTexture) //
    {
        RestartColorRandomizer();
        // clang-format off
        FillTexture3D(pContext, pTexture, Box{ 0u,       32u,   0u,       32u,  0u, TexSize.z}, 0, RandomColor());
        FillTexture3D(pContext, pTexture, Box{32u, TexSize.x,   0u,       32u,  0u, TexSize.z}, 0, RandomColor());
        FillTexture3D(pContext, pTexture, Box{ 0u,       32u,  32u, TexSize.y,  0u, TexSize.z}, 0, RandomColor());
        FillTexture3D(pContext, pTexture, Box{32u, TexSize.x,  32u, TexSize.y,  0u, TexSize.z}, 0, RandomColor());
        // clang-format on

        for (Uint32 Mip = 1, MipLevels = pTexture->GetDesc().MipLevels; Mip < MipLevels; ++Mip)
            FillTexture3DMip(pContext, pTexture, Mip, RandomColor());
    };

    // Draw reference
    {
        RefCntAutoPtr<ITestingSwapChain> pTestingSwapChain(pSwapChain, IID_TestingSwapChain);

        auto pRefTexture = CreateTexture(TexSize, BIND_NONE);
        ASSERT_NE(pRefTexture, nullptr);

        Fill(pRefTexture);
        DrawFSQuadWithTexture(pContext, pPSO, pRefTexture);

        auto pRT = pSwapChain->GetCurrentBackBufferRTV()->GetTexture();

        // Transition to CopySrc state to use in TakeSnapshot()
        StateTransitionDesc Barrier{pRT, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_COPY_SOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);

        pContext->Flush();
        pContext->InvalidateState(); // because TakeSnapshot() will clear state in D3D11

        pTestingSwapChain->TakeSnapshot(pRT);
    }

    const auto BlockSize = SparseMem.StandardBlockSize;

    auto TexAndMem = CreateSparseTextureAndMemory(TexSize, BIND_NONE, 16);
    auto pTexture  = TexAndMem.pTexture;
    ASSERT_NE(pTexture, nullptr);
    ASSERT_NE(pTexture->GetNativeHandle(), 0);
    auto pMemory = TexAndMem.pMemory;
    ASSERT_NE(pMemory, nullptr);

    const auto& TexDesc        = pTexture->GetDesc();
    const auto& TexSparseProps = pTexture->GetSparseProperties();
    CheckTextureSparseProperties(pTexture);
    ASSERT_LE(TexSparseProps.MemorySize, pMemory->GetCapacity());

    auto pFence = CreateFence();

    // bind sparse
    {
        std::vector<SparseTextureMemoryBindRange> BindRanges;

        Uint64 MemOffset = 0;
        for (Uint32 Mip = 0; Mip < TexSparseProps.FirstMipInTail; ++Mip)
        {
            const auto Width  = std::max(1u, TexDesc.Width >> Mip);
            const auto Height = std::max(1u, TexDesc.Height >> Mip);
            const auto Depth  = std::max(1u, TexDesc.Depth >> Mip);
            for (Uint32 z = 0; z < Depth; z += TexSparseProps.TileSize[2])
            {
                for (Uint32 y = 0; y < Height; y += TexSparseProps.TileSize[1])
                {
                    for (Uint32 x = 0; x < Width; x += TexSparseProps.TileSize[0])
                    {
                        BindRanges.emplace_back();
                        auto& Range        = BindRanges.back();
                        Range.MipLevel     = Mip;
                        Range.ArraySlice   = 0;
                        Range.Region.MinX  = x;
                        Range.Region.MaxX  = x + TexSparseProps.TileSize[0];
                        Range.Region.MinY  = y;
                        Range.Region.MaxY  = y + TexSparseProps.TileSize[1];
                        Range.Region.MinZ  = z;
                        Range.Region.MaxZ  = z + TexSparseProps.TileSize[2];
                        Range.MemoryOffset = MemOffset;
                        Range.MemorySize   = BlockSize;
                        Range.pMemory      = pMemory;
                        MemOffset += Range.MemorySize;
                    }
                }
            }
        }

        // Mip tail
        const bool IsMetal = pDevice->GetDeviceInfo().IsMetalDevice();
        for (Uint64 OffsetInMipTail = 0; OffsetInMipTail < TexSparseProps.MipTailSize;)
        {
            BindRanges.emplace_back();
            auto& Range           = BindRanges.back();
            Range.MipLevel        = TexSparseProps.FirstMipInTail;
            Range.ArraySlice      = 0;
            Range.OffsetInMipTail = OffsetInMipTail;
            Range.MemoryOffset    = MemOffset;
            Range.MemorySize      = IsMetal ? TexSparseProps.MipTailSize : BlockSize;
            Range.pMemory         = pMemory;
            MemOffset += Range.MemorySize;
            OffsetInMipTail += Range.MemorySize;
        }

        VERIFY_EXPR(MemOffset <= pMemory->GetCapacity());

        SparseTextureMemoryBind SparseTexBind;
        SparseTexBind.pTexture  = pTexture;
        SparseTexBind.NumRanges = static_cast<Uint32>(BindRanges.size());
        SparseTexBind.pRanges   = BindRanges.data();

        BindSparseMemoryAttribs BindSparseAttrs;
        BindSparseAttrs.NumTextureBinds = 1;
        BindSparseAttrs.pTextureBinds   = &SparseTexBind;

        IFence*      SignalFence = pFence;
        const Uint64 SignalValue = 1;

        if (SignalFence)
        {
            BindSparseAttrs.ppSignalFences     = &SignalFence;
            BindSparseAttrs.pSignalFenceValues = &SignalValue;
            BindSparseAttrs.NumSignalFences    = 1;
        }
        sm_pSparseBindingCtx->BindSparseMemory(BindSparseAttrs);

        if (SignalFence)
            pContext->DeviceWaitForFence(SignalFence, SignalValue);

        Fill(pTexture);
    }

    DrawFSQuadWithTexture(pContext, pPSO, pTexture);

    pSwapChain->Present();
}

#if 0
TEST_F(SparseMemoryTest, LargeBuffer)
{
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;
    const auto  DevType   = pDevice->GetDeviceInfo().Type;

    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_BUFFER) == 0)
    {
        GTEST_SKIP() << "Sparse buffer is not supported by this device";
    }

    // Limits which is queried from API is not valid, x/4 works on all tested devices.
    Uint64 BuffSize = std::max(SparseMem.ResourceSpaceSize >> 2, Uint64{1} << 31);
    Uint32 Stride   = static_cast<Uint32>(std::min(BuffSize, Uint64{1} << 17));

    if (DevType == RENDER_DEVICE_TYPE_D3D11)
    {
        Stride   = 2048;
        BuffSize = std::min(BuffSize, Uint64{UINT32_MAX} * Stride);
    }
    else if (DevType == RENDER_DEVICE_TYPE_D3D12)
    {
        BuffSize = std::min(BuffSize, Uint64{2097152} * Stride); // max supported in D3D12 number of elements
    }

    auto pBuffer = CreateSparseBuffer(BuffSize, BIND_NONE, /*Aliasing*/ false, Stride);
    ASSERT_NE(pBuffer, nullptr);
    ASSERT_NE(pBuffer->GetNativeHandle(), 0);

    LOG_INFO_MESSAGE("Created sparse buffer with size ", pBuffer->GetDesc().Size >> 20, " Mb");
}


TEST_F(SparseMemoryTest, LargeTexture2D)
{
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_2D) == 0)
    {
        GTEST_SKIP() << "Sparse texture 2D is not supported by this device";
    }

    TextureFormatDimensions FmtDims;
    {
        TextureDesc TexDesc;
        TexDesc.Type      = RESOURCE_DIM_TEX_2D;
        TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        TexDesc.Usage     = USAGE_SPARSE;

        FmtDims = pDevice->GetTextureFormatDimensions(TexDesc);
    }

    uint4      TexSize{FmtDims.MaxWidth, FmtDims.MaxHeight, 1u, 1u};
    const auto BPP = 4u;

    if ((Uint64{TexSize.x} * Uint64{TexSize.y} * BPP * 3) / 2 > FmtDims.MaxMemorySize)
        TexSize.y = std::max(1u, static_cast<Uint32>(FmtDims.MaxMemorySize / (Uint64{TexSize.x} * BPP * 3)) * 2);

    auto TexAndMem = CreateSparseTextureAndMemory(TexSize, BIND_NONE, 8);
    auto pTexture  = TexAndMem.pTexture;
    ASSERT_NE(pTexture, nullptr);
    ASSERT_NE(pTexture->GetNativeHandle(), 0);
    auto pMemory = TexAndMem.pMemory;
    ASSERT_NE(pMemory, nullptr);

    const auto& TexSparseProps = pTexture->GetSparseProperties();
    CheckTextureSparseProperties(pTexture);
    ASSERT_LE(TexSparseProps.MemorySize, FmtDims.MaxMemorySize);

    LOG_INFO_MESSAGE("Created sparse 2D texture with dimension ", TexSize.x, "x", TexSize.y, " and size ", TexSparseProps.MemorySize >> 20, " Mb");
}


TEST_F(SparseMemoryTest, LargeTexture2DArray)
{
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_2D_ARRAY_MIP_TAIL) == 0)
    {
        GTEST_SKIP() << "Sparse texture 2D array with mip tail is not supported by this device";
    }

    TextureFormatDimensions FmtDims;
    {
        TextureDesc TexDesc;
        TexDesc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
        TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        TexDesc.Usage     = USAGE_SPARSE;

        FmtDims = pDevice->GetTextureFormatDimensions(TexDesc);
    }

    uint4      TexSize{FmtDims.MaxWidth, FmtDims.MaxHeight, 1u, FmtDims.MaxArraySize};
    const auto BPP           = 4u;
    const auto MaxMemorySize = std::min(FmtDims.MaxMemorySize, SparseMem.ResourceSpaceSize >> 1);

    if ((Uint64{TexSize.x} * Uint64{TexSize.y} * TexSize.w * BPP * 3) / 2 > MaxMemorySize)
        TexSize.y = std::max(1u, static_cast<Uint32>(MaxMemorySize / (Uint64{TexSize.x} * TexSize.w * BPP * 3)) * 2);

    auto TexAndMem = CreateSparseTextureAndMemory(TexSize, BIND_NONE, 8);
    auto pTexture  = TexAndMem.pTexture;
    ASSERT_NE(pTexture, nullptr);
    ASSERT_NE(pTexture->GetNativeHandle(), 0);
    auto pMemory = TexAndMem.pMemory;
    ASSERT_NE(pMemory, nullptr);

    const auto& TexSparseProps = pTexture->GetSparseProperties();
    CheckTextureSparseProperties(pTexture);
    ASSERT_LE(TexSparseProps.MemorySize, FmtDims.MaxMemorySize);

    LOG_INFO_MESSAGE("Created sparse 2D texture array with dimension ", TexSize.x, "x", TexSize.y, ", ", TexSize.w, "layers and size ", TexSparseProps.MemorySize >> 20, " Mb");
}


TEST_F(SparseMemoryTest, LargeTexture3D)
{
    auto*       pEnv      = TestingEnvironment::GetInstance();
    auto*       pDevice   = pEnv->GetDevice();
    const auto& SparseMem = pDevice->GetAdapterInfo().SparseMemory;

    if ((SparseMem.CapFlags & SPARSE_MEMORY_CAP_FLAG_TEXTURE_3D) == 0)
    {
        GTEST_SKIP() << "Sparse texture 3D is not supported by this device";
    }

    TextureFormatDimensions FmtDims;
    {
        TextureDesc TexDesc;
        TexDesc.Type      = RESOURCE_DIM_TEX_3D;
        TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        TexDesc.Usage     = USAGE_SPARSE;

        FmtDims = pDevice->GetTextureFormatDimensions(TexDesc);
    }

    uint4      TexSize{FmtDims.MaxWidth, FmtDims.MaxHeight, FmtDims.MaxDepth, 1u};
    const auto BPP           = 4u;
    const auto MaxMemorySize = std::min(FmtDims.MaxMemorySize, SparseMem.ResourceSpaceSize >> 4);

    if ((Uint64{TexSize.x} * Uint64{TexSize.y} * Uint64{TexSize.z} * BPP * 3) / 2 > MaxMemorySize)
        TexSize.z = std::max(1u, static_cast<Uint32>(MaxMemorySize / (Uint64{TexSize.x} * Uint64{TexSize.y} * BPP * 3)) * 2);

    auto TexAndMem = CreateSparseTextureAndMemory(TexSize, BIND_NONE, 8);
    auto pTexture  = TexAndMem.pTexture;
    ASSERT_NE(pTexture, nullptr);
    ASSERT_NE(pTexture->GetNativeHandle(), 0);
    auto pMemory = TexAndMem.pMemory;
    ASSERT_NE(pMemory, nullptr);

    const auto& TexSparseProps = pTexture->GetSparseProperties();
    CheckTextureSparseProperties(pTexture);
    ASSERT_LE(TexSparseProps.MemorySize, FmtDims.MaxMemorySize);

    LOG_INFO_MESSAGE("Created sparse 3D texture with dimension ", TexSize.x, "x", TexSize.y, "x", TexSize.z, " and size ", TexSparseProps.MemorySize >> 20, " Mb");
}
#endif

// AZ TODO:
//  - depth stencil
//  - multisampled
//  - feedback sampler (dx12, metal?, vk?)

} // namespace
