/*     Copyright 2015-2019 Egor Yusov
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

// The source code in this file is derived from ColorBuffer.cpp and GraphicsCore.cpp developed by Minigraph
// Original source files header:

//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//


#include "pch.h"
#include "d3dx12_win.h"

#include "RenderDeviceD3D12Impl.h"
#include "GenerateMips.h"
#include "CommandContext.h"
#include "TextureViewD3D12Impl.h"
#include "TextureD3D12Impl.h"

#include "GenerateMips/GenerateMipsLinearCS.h"
#include "GenerateMips/GenerateMipsLinearOddCS.h"
#include "GenerateMips/GenerateMipsLinearOddXCS.h"
#include "GenerateMips/GenerateMipsLinearOddYCS.h"
#include "GenerateMips/GenerateMipsGammaCS.h"
#include "GenerateMips/GenerateMipsGammaOddCS.h"
#include "GenerateMips/GenerateMipsGammaOddXCS.h"
#include "GenerateMips/GenerateMipsGammaOddYCS.h"

namespace Diligent
{
    GenerateMipsHelper::GenerateMipsHelper(ID3D12Device *pd3d12Device)
    {
        CD3DX12_ROOT_PARAMETER Params[3];
        Params[0].InitAsConstants(6, 0);
        CD3DX12_DESCRIPTOR_RANGE SRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        Params[1].InitAsDescriptorTable(1, &SRVRange);
        CD3DX12_DESCRIPTOR_RANGE UAVRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0);
        Params[2].InitAsDescriptorTable(1, &UAVRange);
        CD3DX12_STATIC_SAMPLER_DESC SamplerLinearClampDesc(
            0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
        CD3DX12_ROOT_SIGNATURE_DESC RootSigDesc;
        RootSigDesc.NumParameters = _countof(Params);
        RootSigDesc.pParameters = Params;
        RootSigDesc.NumStaticSamplers = 1;
        RootSigDesc.pStaticSamplers = &SamplerLinearClampDesc;
        RootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	    CComPtr<ID3DBlob> signature;
	    CComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&RootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        hr = pd3d12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(m_pGenerateMipsRS), reinterpret_cast<void**>( static_cast<ID3D12RootSignature**>(&m_pGenerateMipsRS)));
        CHECK_D3D_RESULT_THROW(hr, "Failed to create root signature for mipmap generation");

        D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
        PSODesc.pRootSignature = m_pGenerateMipsRS;
        PSODesc.NodeMask = 0;
        PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

#define CreatePSO(PSO, ShaderByteCode) \
        PSODesc.CS.pShaderBytecode = ShaderByteCode;\
        PSODesc.CS.BytecodeLength = sizeof(ShaderByteCode);\
        hr = pd3d12Device->CreateComputePipelineState(&PSODesc, __uuidof(PSO), reinterpret_cast<void**>( static_cast<ID3D12PipelineState**>(&PSO))); \
        CHECK_D3D_RESULT_THROW(hr, "Failed to create Pipeline state for mipmap generation"); \
        PSO->SetName(L"Generate mips PSO");

        CreatePSO(m_pGenerateMipsLinearPSO[0], g_pGenerateMipsLinearCS);
	    CreatePSO(m_pGenerateMipsLinearPSO[1], g_pGenerateMipsLinearOddXCS);
	    CreatePSO(m_pGenerateMipsLinearPSO[2], g_pGenerateMipsLinearOddYCS);
	    CreatePSO(m_pGenerateMipsLinearPSO[3], g_pGenerateMipsLinearOddCS);
	    CreatePSO(m_pGenerateMipsGammaPSO[0], g_pGenerateMipsGammaCS);
	    CreatePSO(m_pGenerateMipsGammaPSO[1], g_pGenerateMipsGammaOddXCS);
	    CreatePSO(m_pGenerateMipsGammaPSO[2], g_pGenerateMipsGammaOddYCS);
	    CreatePSO(m_pGenerateMipsGammaPSO[3], g_pGenerateMipsGammaOddCS);
    }

    void GenerateMipsHelper::GenerateMips(ID3D12Device* pd3d12Device, TextureViewD3D12Impl* pTexView, CommandContext& Ctx)const
    {
        auto& ComputeCtx = Ctx.AsComputeContext();
        ComputeCtx.SetRootSignature(m_pGenerateMipsRS);
        auto* pTexture = pTexView->GetTexture();
        auto* pTexD3D12 = ValidatedCast<TextureD3D12Impl>( pTexture );
        auto& TexDesc = pTexture->GetDesc();
        auto SRVDescriptorHandle = pTexD3D12->GetTexArraySRV();

        if (!pTexD3D12->IsInKnownState())
        {
            LOG_ERROR_MESSAGE("Unable to generate mips for texture '", TexDesc.Name, "' because the texture state is unknown");
            return;
        }

        if (pTexD3D12->IsInKnownState() && !pTexD3D12->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
	        Ctx.TransitionResource(pTexD3D12, RESOURCE_STATE_UNORDERED_ACCESS);

        const auto &ViewDesc = pTexView->GetDesc();
        for (uint32_t TopMip = 0; TopMip < TexDesc.MipLevels - 1; )
        {
            uint32_t SrcWidth  = std::max(TexDesc.Width  >> TopMip, 1u);
            uint32_t SrcHeight = std::max(TexDesc.Height >> TopMip, 1u);
            uint32_t DstWidth  = std::max(SrcWidth  >> 1, 1u);
            uint32_t DstHeight = std::max(SrcHeight >> 1, 1u);

            // Determine if the first downsample is more than 2:1.  This happens whenever
            // the source width or height is odd.
            uint32_t NonPowerOfTwo = (SrcWidth & 1) | (SrcHeight & 1) << 1;
            if (TexDesc.Format == TEX_FORMAT_RGBA8_UNORM_SRGB)
                ComputeCtx.SetPipelineState(m_pGenerateMipsGammaPSO[NonPowerOfTwo]);
            else
                ComputeCtx.SetPipelineState(m_pGenerateMipsLinearPSO[NonPowerOfTwo]);

            // We can downsample up to four times, but if the ratio between levels is not
            // exactly 2:1, we have to shift our blend weights, which gets complicated or
            // expensive.  Maybe we can update the code later to compute sample weights for
            // each successive downsample.  We use _BitScanForward to count number of zeros
            // in the low bits.  Zeros indicate we can divide by two without truncating.
            uint32_t AdditionalMips;
            _BitScanForward((unsigned long*)&AdditionalMips, DstWidth | DstHeight);
            uint32_t NumMips = 1 + (AdditionalMips > 3 ? 3 : AdditionalMips);
            if (TopMip + NumMips > TexDesc.MipLevels - 1)
                NumMips = TexDesc.MipLevels - 1 - TopMip;

            // These are clamped to 1 after computing additional mips because clamped
            // dimensions should not limit us from downsampling multiple times.  (E.g.
            // 16x1 -> 8x1 -> 4x1 -> 2x1 -> 1x1.)
            if (DstWidth == 0)
                DstWidth = 1;
            if (DstHeight == 0)
                DstHeight = 1;

            D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            auto DescriptorAlloc = Ctx.AllocateDynamicGPUVisibleDescriptor(HeapType, 5);
            CommandContext::ShaderDescriptorHeaps Heaps(DescriptorAlloc.GetDescriptorHeap());
            ComputeCtx.SetDescriptorHeaps(Heaps);
            Ctx.GetCommandList()->SetComputeRootDescriptorTable(1, DescriptorAlloc.GetGpuHandle(0));
            Ctx.GetCommandList()->SetComputeRootDescriptorTable(2, DescriptorAlloc.GetGpuHandle(1));
            struct RootCBData
            {
                Uint32 SrcMipLevel;	    // Texture level of source mip
                Uint32 NumMipLevels;	// Number of OutMips to write: [1, 4]
                Uint32 FirstArraySlice;
                Uint32 Dummy;
                float TexelSize[2];	    // 1.0 / OutMip1.Dimensions
            }CBData = { TopMip, NumMips, ViewDesc.FirstArraySlice, 0, 1.0f / static_cast<float>(DstWidth), 1.0f / static_cast<float>(DstHeight) };
            Ctx.GetCommandList()->SetComputeRoot32BitConstants(0, 6, &CBData, 0);

            // TODO: Shouldn't we transition top mip to shader resource state?
            D3D12_CPU_DESCRIPTOR_HANDLE DstDescriptorRange = DescriptorAlloc.GetCpuHandle();
            const Uint32 MaxMipsHandledByCS = 4; // Max number of mip levels processed by one CS shader invocation
            UINT DstRangeSize = 1 + MaxMipsHandledByCS;
            D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRanges[5] = {};
            SrcDescriptorRanges[0] = SRVDescriptorHandle;
            UINT SrcRangeSizes[5] = { 1,1,1,1,1 };
            // On Resource Binding Tier 2 hardware, all descriptor tables of type CBV and UAV declared in the set 
            // Root Signature must be populated and initialized, even if the shaders do not need the descriptor.
            // So we must populate all 4 slots even though we may actually process less than 4 mip levels
            // Copy top mip level UAV descriptor handle to all unused slots
            for (Uint32 u = 0; u < MaxMipsHandledByCS; ++u)
                SrcDescriptorRanges[1 + u] = pTexD3D12->GetMipLevelUAV(std::min(TopMip + u + 1, TexDesc.MipLevels - 1));

            pd3d12Device->CopyDescriptors(1, &DstDescriptorRange, &DstRangeSize, 1 + MaxMipsHandledByCS, SrcDescriptorRanges, SrcRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            ComputeCtx.Dispatch((DstWidth + 7) / 8, (DstHeight + 7) / 8, ViewDesc.NumArraySlices);

            Ctx.InsertUAVBarrier(pTexD3D12->GetD3D12Resource());

            TopMip += NumMips;
        }
    }
}
