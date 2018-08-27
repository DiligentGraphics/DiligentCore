/*     Copyright 2015-2018 Egor Yusov
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

#include "pch.h"
#include <sstream>
#include "GenerateMipsVkHelper.h"
#include "RenderDeviceVkImpl.h"
#include "DeviceContextVkImpl.h"
#include "TextureViewVkImpl.h"
#include "TextureVkImpl.h"
#include "MapHelper.h"
#include "../../GraphicsTools/include/ShaderMacroHelper.h"
#include "../../GraphicsTools/include/CommonlyUsedStates.h"


static const char* g_GenerateMipsCSSource = 
{
    #include "../shaders/GenerateMipsCS_inc.h"
};

namespace Diligent
{
    void GenerateMipsVkHelper::GetGlImageFormat(const TextureFormatAttribs& FmtAttribs, std::array<char, 16>& GlFmt)
    {
        size_t pos = 0;
        GlFmt[pos++] = 'r';
        if(FmtAttribs.NumComponents >= 2)
            GlFmt[pos++] = 'g';
        if(FmtAttribs.NumComponents >= 3)
            GlFmt[pos++] = 'b';
        if(FmtAttribs.NumComponents >= 4)
            GlFmt[pos++] = 'a';
        VERIFY_EXPR(FmtAttribs.NumComponents <= 4);
        auto ComponentSize = Uint32{FmtAttribs.ComponentSize} * 8;
        int pow10 = 1;
        while(ComponentSize / (10*pow10) != 0)
            pow10 *= 10;
        VERIFY_EXPR(ComponentSize !=0);
        while(ComponentSize != 0)
        {
            char digit = static_cast<char>(ComponentSize/pow10);
            GlFmt[pos++] = '0' + digit;
            ComponentSize -= digit * pow10;
            pow10 /= 10;
        }

        switch(FmtAttribs.ComponentType)
        {
            case COMPONENT_TYPE_FLOAT:
                GlFmt[pos++] = 'f';
                break;

            case COMPONENT_TYPE_UNORM:
            case COMPONENT_TYPE_UNORM_SRGB:
                // No suffix
                break;

            case COMPONENT_TYPE_SNORM:
                GlFmt[pos++] = '_';
                GlFmt[pos++] = 's';
                GlFmt[pos++] = 'n';
                GlFmt[pos++] = 'o';
                GlFmt[pos++] = 'r';
                GlFmt[pos++] = 'm';
                break;

            case COMPONENT_TYPE_SINT:
                GlFmt[pos++] = 'i';
                break;

            case COMPONENT_TYPE_UINT:
                GlFmt[pos++] = 'u';
                GlFmt[pos++] = 'i';
                break;

            default:
                UNSUPPORTED("Unsupported component type");
        }

        GlFmt[pos] = 0;
    }

    std::array<RefCntAutoPtr<IPipelineState>, 4> GenerateMipsVkHelper::CreatePSOs(TEXTURE_FORMAT Fmt)
    {
        ShaderCreationAttribs CSCreateAttribs;
        std::array<RefCntAutoPtr<IPipelineState>, 4> PSOs;
        
        CSCreateAttribs.Source = g_GenerateMipsCSSource;
        CSCreateAttribs.EntryPoint = "main";
        CSCreateAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_GLSL;
        CSCreateAttribs.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        CSCreateAttribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_DYNAMIC;
        
        ShaderVariableDesc VarDesc("CB", SHADER_VARIABLE_TYPE_STATIC);
        CSCreateAttribs.Desc.VariableDesc = &VarDesc;
        CSCreateAttribs.Desc.NumVariables = 1;

        const StaticSamplerDesc StaticSampler("SrcMip", Sam_LinearClamp);
        CSCreateAttribs.Desc.StaticSamplers = &StaticSampler;
        CSCreateAttribs.Desc.NumStaticSamplers = 1;

        const auto& FmtAttribs = GetTextureFormatAttribs(Fmt);
        bool IsGamma = FmtAttribs.ComponentType == COMPONENT_TYPE_UNORM_SRGB;
        std::array<char, 16> GlFmt;
        GetGlImageFormat(FmtAttribs, GlFmt);

        for(Uint32 NonPowOfTwo=0; NonPowOfTwo < 4; ++NonPowOfTwo)
        {
            ShaderMacroHelper Macros;
            Macros.AddShaderMacro("NON_POWER_OF_TWO", NonPowOfTwo);
            Macros.AddShaderMacro("CONVERT_TO_SRGB", IsGamma);
            Macros.AddShaderMacro("IMG_FORMAT", GlFmt.data());

            Macros.Finalize();
            CSCreateAttribs.Macros = Macros;

            std::stringstream name_ss;
            name_ss << "Generate mips " << GlFmt.data();
            switch(NonPowOfTwo)
            {
                case 0: name_ss << " even"; break;
                case 1: name_ss << " odd X"; break;
                case 2: name_ss << " odd Y"; break;
                case 3: name_ss << " odd XY"; break;
                default: UNEXPECTED("Unexpected value");
            }
            auto name = name_ss.str();
            CSCreateAttribs.Desc.Name = name.c_str();
            RefCntAutoPtr<IShader> pCS;

            m_DeviceVkImpl.CreateShader(CSCreateAttribs, &pCS);
            PipelineStateDesc PSODesc;
            PSODesc.IsComputePipeline = true;
            PSODesc.Name = name.c_str();
            PSODesc.ComputePipeline.pCS = pCS;
            pCS->GetShaderVariable("CB")->Set(m_ConstantsCB);
            m_DeviceVkImpl.CreatePipelineState(PSODesc, &PSOs[NonPowOfTwo]);
        }

        return PSOs;
    }

    GenerateMipsVkHelper::GenerateMipsVkHelper(RenderDeviceVkImpl& DeviceVkImpl) :
        m_DeviceVkImpl(DeviceVkImpl)
    {
        BufferDesc ConstantsCBDesc;
        ConstantsCBDesc.Name = "Constants CB buffer";
        ConstantsCBDesc.BindFlags = BIND_UNIFORM_BUFFER;
        ConstantsCBDesc.Usage = USAGE_DYNAMIC;
        ConstantsCBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        ConstantsCBDesc.uiSizeInBytes = 32;
        DeviceVkImpl.CreateBuffer(ConstantsCBDesc, BufferData(), &m_ConstantsCB);

        FindPSOs(TEX_FORMAT_RGBA8_UNORM);
        FindPSOs(TEX_FORMAT_BGRA8_UNORM);
        FindPSOs(TEX_FORMAT_RGBA8_UNORM_SRGB);
        FindPSOs(TEX_FORMAT_BGRA8_UNORM_SRGB);
    }

    void GenerateMipsVkHelper::CreateSRB(IShaderResourceBinding** ppSRB)
    {
        // All PSOs are compatible
        auto& PSO = FindPSOs(TEX_FORMAT_RGBA8_UNORM);
        PSO[0]->CreateShaderResourceBinding(ppSRB);
    }

    std::array<RefCntAutoPtr<IPipelineState>, 4>& GenerateMipsVkHelper::FindPSOs(TEXTURE_FORMAT Fmt)
    {
        std::lock_guard<std::mutex> Lock(m_PSOMutex);
        auto it = m_PSOHash.find(Fmt);
        if(it == m_PSOHash.end())
            it = m_PSOHash.emplace(Fmt, CreatePSOs(Fmt)).first;
        return it->second;
    }

    void GenerateMipsVkHelper::GenerateMips(TextureViewVkImpl& TexView, DeviceContextVkImpl& Ctx, IShaderResourceBinding& SRB)
    {
        auto* pTexVk = TexView.GetTexture<TextureVkImpl>();
        const auto& TexDesc = pTexVk->GetDesc();
        const auto& ViewDesc = TexView.GetDesc();
        auto* pSrcMipVar = SRB.GetVariable(SHADER_TYPE_COMPUTE, "SrcMip");
        auto* pOutMipVar = SRB.GetVariable(SHADER_TYPE_COMPUTE, "OutMip");

        auto& PSOs = FindPSOs(ViewDesc.Format);

        const auto& FmtAttribs = GetTextureFormatAttribs(TexDesc.Format);
        VkImageSubresourceRange SubresRange;
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH)
            SubresRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else if (FmtAttribs.ComponentType == COMPONENT_TYPE_DEPTH_STENCIL)
        {
            // If image has a depth / stencil format with both depth and stencil components, then the 
            // aspectMask member of subresourceRange must include both VK_IMAGE_ASPECT_DEPTH_BIT and 
            // VK_IMAGE_ASPECT_STENCIL_BIT (6.7.3)
            SubresRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        else
            SubresRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        SubresRange.baseArrayLayer = 0;
        SubresRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        
        auto CurrLayout = pTexVk->GetLayout();

        // Transition the lowest mip level to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        SubresRange.baseMipLevel = 0;
        SubresRange.levelCount = 1;
        if (CurrLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            Ctx.TransitionImageLayout(*pTexVk, CurrLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SubresRange);

        for (uint32_t TopMip = 0; TopMip < TexDesc.MipLevels - 1; )
        {
            // In Vulkan all subresources of a view must be transitioned to the same layout, so
            // we can't bind the entire texture and have to bind single mip level at a time
            pSrcMipVar->Set(pTexVk->GetMipLevelSRV(TopMip));

            uint32_t SrcWidth  = std::max(TexDesc.Width  >> TopMip, 1u);
            uint32_t SrcHeight = std::max(TexDesc.Height >> TopMip, 1u);
            uint32_t DstWidth  = std::max(SrcWidth  >> 1, 1u);
            uint32_t DstHeight = std::max(SrcHeight >> 1, 1u);

            // Determine if the first downsample is more than 2:1.  This happens whenever
            // the source width or height is odd.
            uint32_t NonPowerOfTwo = (SrcWidth & 1) | (SrcHeight & 1) << 1;
            Ctx.SetPipelineState(PSOs[NonPowerOfTwo]);

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

            {
                struct CBData
                {
                    Int32 SrcMipLevel;  // Texture level of source mip
                    Int32 NumMipLevels; // Number of OutMips to write: [1, 4]
                    Int32 ArraySlice;
                    Int32 Dummy;
                    float TexelSize[2];	    // 1.0 / OutMip1.Dimensions
                };
                MapHelper<CBData> MappedData(&Ctx, m_ConstantsCB, MAP_WRITE, MAP_FLAG_DISCARD);
                    
                *MappedData = 
                {
                    static_cast<Int32>(TopMip),
                    static_cast<Int32>(NumMips),
                    static_cast<Int32>(ViewDesc.FirstArraySlice),
                    0,
                    1.0f / static_cast<float>(DstWidth),
                    1.0f / static_cast<float>(DstHeight)
                };
            }

            constexpr const Uint32 MaxMipsHandledByCS = 4; // Max number of mip levels processed by one CS shader invocation
            std::array<IDeviceObject*, MaxMipsHandledByCS> MipLevelUAVs;
            for (Uint32 u = 0; u < MaxMipsHandledByCS; ++u)
                MipLevelUAVs[u] = pTexVk->GetMipLevelUAV(std::min(TopMip + u + 1, TexDesc.MipLevels - 1));
            pOutMipVar->SetArray(MipLevelUAVs.data(), 0, MaxMipsHandledByCS);

            SubresRange.baseMipLevel = TopMip + 1;
            SubresRange.levelCount = std::min(4u, TexDesc.MipLevels - (TopMip + 1));
            if (CurrLayout != VK_IMAGE_LAYOUT_GENERAL)
                Ctx.TransitionImageLayout(*pTexVk, CurrLayout, VK_IMAGE_LAYOUT_GENERAL, SubresRange);

            Ctx.CommitShaderResources(&SRB, 0);
            DispatchComputeAttribs DispatchAttrs((DstWidth + 7) / 8, (DstHeight + 7) / 8, ViewDesc.NumArraySlices);
            Ctx.DispatchCompute(DispatchAttrs);

            Ctx.TransitionImageLayout(*pTexVk, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SubresRange);

            TopMip += NumMips;
        }

        // All mip levels are now in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL state
        pTexVk->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}
