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

#include "SPIRVShaderResources.h"
#include "spirv_cross.hpp"
#include "ShaderBase.h"
#include "GraphicsAccessories.h"

namespace Diligent
{

template<typename Type>
Type GetResourceArraySize(const spirv_cross::Compiler &Compiler,
                          const spirv_cross::Resource &Res)
{
    const auto& type = Compiler.get_type(Res.type_id);
    uint32_t arrSize = 1;
    if(!type.array.empty())
    {
        // https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide#querying-array-types
        VERIFY(type.array.size() == 1, "Only one-dimensional arrays are currently supported");
        arrSize = type.array[0];
    }
    VERIFY(arrSize <= std::numeric_limits<Type>::max(), "Array size exceeds maximum representable value ", std::numeric_limits<Type>::max());
    return static_cast<Type>(arrSize);
}

static uint32_t GetDecorationOffset(const spirv_cross::Compiler &Compiler,
                                    const spirv_cross::Resource &Res,
                                    spv::Decoration Decoration)
{
    VERIFY(Compiler.has_decoration(Res.id, Decoration), "Res \'", Res.name, "\' has no requested decoration");
    uint32_t offset = 0;
    auto declared = Compiler.get_binary_offset_for_decoration(Res.id, Decoration, offset);
    VERIFY(declared, "Requested decoration is not declared");
    return offset;
}

SPIRVShaderResourceAttribs::SPIRVShaderResourceAttribs(const spirv_cross::Compiler&  Compiler,
                                                       const spirv_cross::Resource&  Res, 
                                                       ResourceType                  _Type, 
                                                       SHADER_VARIABLE_TYPE          _VarType,
                                                       Int32                         _StaticSamplerInd) :
    Name(Res.name),
    ArraySize(GetResourceArraySize<decltype(ArraySize)>(Compiler, Res)),
    BindingDecorationOffset(GetDecorationOffset(Compiler, Res, spv::Decoration::DecorationBinding)),
    DescriptorSetDecorationOffset(GetDecorationOffset(Compiler, Res, spv::Decoration::DecorationDescriptorSet)),
    Type(_Type),
    VarType(_VarType),
    StaticSamplerInd(static_cast<decltype(StaticSamplerInd)>(_StaticSamplerInd))
{
    VERIFY(_StaticSamplerInd >= std::numeric_limits<decltype(StaticSamplerInd)>::min() && 
           _StaticSamplerInd <= std::numeric_limits<decltype(StaticSamplerInd)>::max(), "Static sampler index is out of representable range" );
}

Int32 FindStaticSampler(const ShaderDesc& shaderDesc, const char* SamplerName)
{
    for(Uint32 s=0; s < shaderDesc.NumStaticSamplers; ++s)
    {
        const auto& StSam = shaderDesc.StaticSamplers[s];
        if(strcmp(SamplerName, StSam.TextureName) == 0)
            return s;
    }

    return -1;
}

SPIRVShaderResources::SPIRVShaderResources(IMemoryAllocator&         Allocator, 
                                           IRenderDevice*            pRenderDevice,
                                           std::vector<uint32_t>     spirv_binary,
                                           const ShaderDesc&         shaderDesc) :
    m_MemoryBuffer(nullptr, STDDeleterRawMem<void>(Allocator)),
    m_ShaderType(shaderDesc.ShaderType)
{
    // https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide
    spirv_cross::Compiler Compiler(std::move(spirv_binary));

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = Compiler.get_shader_resources();

    Initialize(Allocator, 
               static_cast<Uint32>(resources.uniform_buffers.size()),
               static_cast<Uint32>(resources.storage_buffers.size()),
               static_cast<Uint32>(resources.storage_images.size()),
               static_cast<Uint32>(resources.sampled_images.size()),
               static_cast<Uint32>(resources.atomic_counters.size()),
               static_cast<Uint32>(resources.separate_images.size()),
               static_cast<Uint32>(resources.separate_samplers.size()),
               shaderDesc.NumStaticSamplers);

    {
        Uint32 CurrUB = 0;
        for (const auto &UB : resources.uniform_buffers)
        {
            auto VarType = GetShaderVariableType(UB.name.c_str(), shaderDesc.DefaultVariableType, shaderDesc.VariableDesc, shaderDesc.NumVariables);
            new (&GetUB(CurrUB++)) SPIRVShaderResourceAttribs(Compiler, UB, SPIRVShaderResourceAttribs::ResourceType::UniformBuffer, VarType, false);
        }
        VERIFY_EXPR(CurrUB == GetNumUBs());
    }

    {
        Uint32 CurrSB = 0;
        for (const auto &SB : resources.storage_buffers)
        {
            auto VarType = GetShaderVariableType(SB.name.c_str(), shaderDesc.DefaultVariableType, shaderDesc.VariableDesc, shaderDesc.NumVariables);
            new (&GetSB(CurrSB++)) SPIRVShaderResourceAttribs(Compiler, SB, SPIRVShaderResourceAttribs::ResourceType::StorageBuffer, VarType, false);
        }
        VERIFY_EXPR(CurrSB == GetNumSBs());
    }

    {
        Uint32 CurrSmplImg = 0;
        for (const auto &SmplImg : resources.sampled_images)
        {
            auto VarType = GetShaderVariableType(SmplImg.name.c_str(), shaderDesc.DefaultVariableType, shaderDesc.VariableDesc, shaderDesc.NumVariables);
            auto StaticSamplerInd = FindStaticSampler(shaderDesc, SmplImg.name.c_str());
            const auto& type = Compiler.get_type(SmplImg.type_id);
            auto ResType = type.image.dim == spv::DimBuffer ?
                SPIRVShaderResourceAttribs::ResourceType::UniformTexelBuffer :
                SPIRVShaderResourceAttribs::ResourceType::SampledImage;
            new (&GetSmplImg(CurrSmplImg++)) SPIRVShaderResourceAttribs(Compiler, SmplImg, ResType, VarType, StaticSamplerInd);
        }
        VERIFY_EXPR(CurrSmplImg == GetNumSmplImgs()); 
    }

    {
        Uint32 CurrImg = 0;
        for (const auto &Img : resources.storage_images)
        {
            auto VarType = GetShaderVariableType(Img.name.c_str(), shaderDesc.DefaultVariableType, shaderDesc.VariableDesc, shaderDesc.NumVariables);
            const auto& type = Compiler.get_type(Img.type_id);
            auto ResType = type.image.dim == spv::DimBuffer ?
                SPIRVShaderResourceAttribs::ResourceType::StorageTexelBuffer :
                SPIRVShaderResourceAttribs::ResourceType::StorageImage;
            new (&GetImg(CurrImg++)) SPIRVShaderResourceAttribs(Compiler, Img, ResType, VarType, false);
        }
        VERIFY_EXPR(CurrImg == GetNumImgs());
    }

    {
        Uint32 CurrAC = 0;
        for (const auto &AC : resources.atomic_counters)
        {
            auto VarType = GetShaderVariableType(AC.name.c_str(), shaderDesc.DefaultVariableType, shaderDesc.VariableDesc, shaderDesc.NumVariables);
            new (&GetAC(CurrAC++)) SPIRVShaderResourceAttribs(Compiler, AC, SPIRVShaderResourceAttribs::ResourceType::AtomicCounter, VarType, false);
        }
        VERIFY_EXPR(CurrAC == GetNumACs());
    }

    {
        Uint32 CurrSepImg = 0;
        for (const auto &SepImg : resources.separate_images)
        {
            auto VarType = GetShaderVariableType(SepImg.name.c_str(), shaderDesc.DefaultVariableType, shaderDesc.VariableDesc, shaderDesc.NumVariables);
            new (&GetSepImg(CurrSepImg++)) SPIRVShaderResourceAttribs(Compiler, SepImg, SPIRVShaderResourceAttribs::ResourceType::SeparateImage, VarType, false);
        }
        VERIFY_EXPR(CurrSepImg == GetNumSepImgs());
    }

    {
        Uint32 CurrSepSmpl = 0;
        for (const auto &SepSam : resources.separate_samplers)
        {
            auto VarType = GetShaderVariableType(SepSam.name.c_str(), shaderDesc.DefaultVariableType, shaderDesc.VariableDesc, shaderDesc.NumVariables);
            auto StaticSamplerInd = FindStaticSampler(shaderDesc, SepSam.name.c_str());
            new (&GetSepSmpl(CurrSepSmpl++)) SPIRVShaderResourceAttribs(Compiler, SepSam, SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, VarType, StaticSamplerInd);
        }
        VERIFY_EXPR(CurrSepSmpl == GetNumSepSmpls());
    }


    for (Uint32 s = 0; s < m_NumStaticSamplers; ++s)
    {
        RefCntAutoPtr<ISampler> &pStaticSampler = GetStaticSampler(s);
        new (std::addressof(pStaticSampler)) RefCntAutoPtr<ISampler>();
        pRenderDevice->CreateSampler(shaderDesc.StaticSamplers[s].Desc, &pStaticSampler);
    }

#ifdef _DEBUG
    if (shaderDesc.NumVariables != 0)
    {
        for (Uint32 v = 0; v < shaderDesc.NumVariables; ++v)
        {
            bool VariableFound = false;
            const auto *VarName = shaderDesc.VariableDesc[v].Name;
            auto VarType = shaderDesc.VariableDesc[v].Type;

            for (Uint32 res = 0; res < GetTotalResources(); ++res)
            {
                const auto &ResAttribs = GetResource(res);
                if (ResAttribs.Name.compare(VarName) == 0)
                {
                    VariableFound = true;
                    break;
                }
            }
            if (!VariableFound)
            {
                LOG_WARNING_MESSAGE("Variable '", VarName, "' labeled as ", GetShaderVariableTypeLiteralName(VarType), " not found in shader \'", shaderDesc.Name, "'");
            }
        }
    }

    if (shaderDesc.NumStaticSamplers != 0)
    {
        for (Uint32 s = 0; s < shaderDesc.NumStaticSamplers; ++s)
        {
            bool SamplerFound = false;
            const auto *SamName = shaderDesc.StaticSamplers[s].TextureName;
            for (Uint32 i = 0; i < GetNumSmplImgs(); ++i)
            {
                const auto &SmplImg = GetSmplImg(i);
                if (SmplImg.Name.compare(SamName) == 0)
                {
                    SamplerFound = true;
                    break;
                }
            }

            if(SamplerFound)
                continue;

            for (Uint32 i = 0; i < GetNumSepSmpls(); ++i)
            {
                const auto &SepSmpl = GetSepSmpl(i);
                if (SepSmpl.Name.compare(SamName) == 0)
                {
                    SamplerFound = true;
                    break;
                }
            }

            if (!SamplerFound)
            {
                LOG_WARNING_MESSAGE("Static sampler '", SamName, "' not found in shader \'", shaderDesc.Name, "'");
            }
        }
    }
#endif
}

void SPIRVShaderResources::Initialize(IMemoryAllocator& Allocator, 
                                      Uint32            NumUBs, 
                                      Uint32            NumSBs, 
                                      Uint32            NumImgs, 
                                      Uint32            NumSmplImgs, 
                                      Uint32            NumACs,
                                      Uint32            NumSepImgs, 
                                      Uint32            NumSepSmpls, 
                                      Uint32            NumStaticSamplers)
{
    VERIFY(&m_MemoryBuffer.get_deleter().m_Allocator == &Allocator, "Incosistent allocators provided");

    static constexpr Uint16 UniformBufferOffset = 0;

    const auto MaxOffset = static_cast<Uint32>(std::numeric_limits<OffsetType>::max());
    VERIFY(UniformBufferOffset + NumUBs <= MaxOffset, "Max offset exceeded");
    m_StorageBufferOffset = UniformBufferOffset + static_cast<OffsetType>(NumUBs);

    VERIFY(m_StorageBufferOffset + NumSBs <= MaxOffset, "Max offset exceeded");
    m_StorageImageOffset = m_StorageBufferOffset + static_cast<OffsetType>(NumSBs);

    VERIFY(m_StorageImageOffset + NumImgs <= MaxOffset, "Max offset exceeded");
    m_SampledImageOffset = m_StorageImageOffset + static_cast<OffsetType>(NumImgs);

    VERIFY(m_SampledImageOffset + NumSmplImgs <= MaxOffset, "Max offset exceeded");
    m_AtomicCounterOffset = m_SampledImageOffset + static_cast<OffsetType>(NumSmplImgs);

    VERIFY(m_AtomicCounterOffset + NumACs <= MaxOffset, "Max offset exceeded");
    m_SeparateImageOffset = m_AtomicCounterOffset + static_cast<OffsetType>(NumACs);

    VERIFY(m_SeparateImageOffset + NumSepImgs <= MaxOffset, "Max offset exceeded");
    m_SeparateSamplerOffset = m_SeparateImageOffset + static_cast<OffsetType>(NumSepImgs);

    VERIFY(m_SeparateSamplerOffset + NumSepSmpls <= MaxOffset, "Max offset exceeded");
    m_TotalResources = m_SeparateSamplerOffset + static_cast<OffsetType>(NumSepSmpls);

    VERIFY(m_NumStaticSamplers <= MaxOffset, "Max offset exceeded");
    m_NumStaticSamplers = static_cast<OffsetType>(NumStaticSamplers);

    auto MemorySize = m_TotalResources * sizeof(SPIRVShaderResourceAttribs) + m_NumStaticSamplers * sizeof(RefCntAutoPtr<ISampler>);

    VERIFY_EXPR(GetNumUBs()      == NumUBs);
    VERIFY_EXPR(GetNumSBs()      == NumSBs);
    VERIFY_EXPR(GetNumImgs()     == NumImgs);
    VERIFY_EXPR(GetNumSmplImgs() == NumSmplImgs);
    VERIFY_EXPR(GetNumACs()      == NumACs);
    VERIFY_EXPR(GetNumSepImgs()  == NumSepImgs);
    VERIFY_EXPR(GetNumSepSmpls() == NumSepSmpls);

    if (MemorySize)
    {
        auto *pRawMem = Allocator.Allocate(MemorySize, "Memory for shader resources", __FILE__, __LINE__);
        m_MemoryBuffer.reset(pRawMem);
    }
}

SPIRVShaderResources::~SPIRVShaderResources()
{
    for (Uint32 n = 0; n < GetNumUBs(); ++n)
        GetUB(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSBs(); ++n)
        GetSB(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumImgs(); ++n)
        GetImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSmplImgs(); ++n)
        GetSmplImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumACs(); ++n)
        GetAC(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSepImgs(); ++n)
        GetSepImg(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumSepSmpls(); ++n)
        GetSepSmpl(n).~SPIRVShaderResourceAttribs();

    for (Uint32 n = 0; n < GetNumStaticSamplers(); ++n)
        GetStaticSampler(n).~RefCntAutoPtr<ISampler>();
}

void SPIRVShaderResources::CountResources(const SHADER_VARIABLE_TYPE *AllowedVarTypes,
                                          Uint32 NumAllowedTypes, 
                                          Uint32& NumUBs, 
                                          Uint32& NumSBs, 
                                          Uint32& NumImgs, 
                                          Uint32& NumSmplImgs, 
                                          Uint32& NumACs, 
                                          Uint32& NumSepImgs,
                                          Uint32& NumSepSmpls)const noexcept
{
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    NumUBs      = 0;
    NumSBs      = 0;
    NumImgs     = 0;
    NumSmplImgs = 0;
    NumACs      = 0;
    NumSepImgs  = 0;
    NumSepSmpls = 0;

    ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const SPIRVShaderResourceAttribs &UB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(UB.VarType, AllowedTypeBits));
            ++NumUBs;
        },
        [&](const SPIRVShaderResourceAttribs& SB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SB.VarType, AllowedTypeBits));
            ++NumSBs;
        },
        [&](const SPIRVShaderResourceAttribs &Img, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(Img.VarType, AllowedTypeBits));
            ++NumImgs;
        },
        [&](const SPIRVShaderResourceAttribs &SmplImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SmplImg.VarType, AllowedTypeBits));
            ++NumSmplImgs;
        },
        [&](const SPIRVShaderResourceAttribs &AC, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(AC.VarType, AllowedTypeBits));
            ++NumACs;
        },
        [&](const SPIRVShaderResourceAttribs &SepImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepImg.VarType, AllowedTypeBits));
            ++NumSepImgs;
        },
        [&](const SPIRVShaderResourceAttribs &SepSmpl, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepSmpl.VarType, AllowedTypeBits));
            ++NumSepSmpls;
        }
    );
}

}
