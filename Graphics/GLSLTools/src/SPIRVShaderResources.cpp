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
    uint32_t arrSize = !type.array.empty() ? type.array[0] : 1;
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

SPIRVShaderResourceAttribs::SPIRVShaderResourceAttribs(const spirv_cross::Compiler &Compiler,
                                                       const spirv_cross::Resource &Res, 
                                                       ResourceType _Type, 
                                                       SHADER_VARIABLE_TYPE _VarType) :
    Name(Res.name),
    ArraySize(GetResourceArraySize<decltype(ArraySize)>(Compiler, Res)),
    BindingDecorationOffset(GetDecorationOffset(Compiler, Res, spv::Decoration::DecorationBinding)),
    DescriptorSetDecorationOffset(GetDecorationOffset(Compiler, Res, spv::Decoration::DecorationDescriptorSet)),
    Type(_Type),
    VarType(_VarType)
{
}

SPIRVShaderResources::SPIRVShaderResources(IMemoryAllocator &Allocator, 
                                           SHADER_TYPE ShaderType, 
                                           std::vector<uint32_t> spirv_binary,
                                           SHADER_VARIABLE_TYPE DefaultVariableType,
                                           const ShaderVariableDesc *VariableDesc,
                                           Uint32 NumVars,
                                           const char *ShaderName) :
    m_MemoryBuffer(nullptr, STDDeleterRawMem<void>(Allocator)),
    m_ShaderType(ShaderType)
{
    spirv_cross::Compiler Compiler(spirv_binary);

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = Compiler.get_shader_resources();

    Initialize(Allocator, 
               static_cast<Uint32>(resources.uniform_buffers.size()),
               static_cast<Uint32>(resources.storage_buffers.size()),
               static_cast<Uint32>(resources.storage_images.size()),
               static_cast<Uint32>(resources.sampled_images.size()),
               static_cast<Uint32>(resources.atomic_counters.size()),
               static_cast<Uint32>(resources.separate_images.size()),
               static_cast<Uint32>(resources.separate_samplers.size())
               );

    {
        Uint32 CurrUB = 0;
        for (const auto &UB : resources.uniform_buffers)
        {
            auto VarType = GetShaderVariableType(UB.name.c_str(), DefaultVariableType, VariableDesc, NumVars);
            new (&GetUB(CurrUB++)) SPIRVShaderResourceAttribs(Compiler, UB, SPIRVShaderResourceAttribs::ResourceType::UniformBuffer, VarType);
        }
        VERIFY_EXPR(CurrUB == GetNumUBs());
    }

    {
        Uint32 CurrSB = 0;
        for (const auto &SB : resources.storage_buffers)
        {
            auto VarType = GetShaderVariableType(SB.name.c_str(), DefaultVariableType, VariableDesc, NumVars);
            new (&GetSB(CurrSB++)) SPIRVShaderResourceAttribs(Compiler, SB, SPIRVShaderResourceAttribs::ResourceType::StorageBuffer, VarType);
        }
        VERIFY_EXPR(CurrSB == GetNumSBs());
    }

    {
        Uint32 CurrSmplImg = 0;
        for (const auto &SmplImg : resources.sampled_images)
        {
            auto VarType = GetShaderVariableType(SmplImg.name.c_str(), DefaultVariableType, VariableDesc, NumVars);
            new (&GetSmplImg(CurrSmplImg++)) SPIRVShaderResourceAttribs(Compiler, SmplImg, SPIRVShaderResourceAttribs::ResourceType::SampledImage, VarType);
        }
        VERIFY_EXPR(CurrSmplImg == GetNumSmplImgs()); 
    }

    {
        Uint32 CurrImg = 0;
        for (const auto &Img : resources.storage_images)
        {
            auto VarType = GetShaderVariableType(Img.name.c_str(), DefaultVariableType, VariableDesc, NumVars);
            new (&GetImg(CurrImg++)) SPIRVShaderResourceAttribs(Compiler, Img, SPIRVShaderResourceAttribs::ResourceType::StorageImage, VarType);
        }
        VERIFY_EXPR(CurrImg == GetNumImgs());
    }

    {
        Uint32 CurrAC = 0;
        for (const auto &AC : resources.atomic_counters)
        {
            auto VarType = GetShaderVariableType(AC.name.c_str(), DefaultVariableType, VariableDesc, NumVars);
            new (&GetAC(CurrAC++)) SPIRVShaderResourceAttribs(Compiler, AC, SPIRVShaderResourceAttribs::ResourceType::AtomicCounter, VarType);
        }
        VERIFY_EXPR(CurrAC == GetNumACs());
    }

    {
        Uint32 CurrSepImg = 0;
        for (const auto &SepImg : resources.separate_images)
        {
            auto VarType = GetShaderVariableType(SepImg.name.c_str(), DefaultVariableType, VariableDesc, NumVars);
            new (&GetSepImg(CurrSepImg++)) SPIRVShaderResourceAttribs(Compiler, SepImg, SPIRVShaderResourceAttribs::ResourceType::SeparateImage, VarType);
        }
        VERIFY_EXPR(CurrSepImg == GetNumSepImgs());
    }

    {
        Uint32 CurrSepSmpl = 0;
        for (const auto &SepSam : resources.separate_samplers)
        {
            auto VarType = GetShaderVariableType(SepSam.name.c_str(), DefaultVariableType, VariableDesc, NumVars);
            new (&GetSepSmpl(CurrSepSmpl++)) SPIRVShaderResourceAttribs(Compiler, SepSam, SPIRVShaderResourceAttribs::ResourceType::SeparateSampler, VarType);
        }
        VERIFY_EXPR(CurrSepSmpl == GetNumSepSmpls());
    }

#ifdef _DEBUG
    if (VariableDesc != nullptr || NumVars != 0)
    {
        for (Uint32 v = 0; v < NumVars; ++v)
        {
            bool VariableFound = false;
            const auto *VarName = VariableDesc[v].Name;
            auto VarType = VariableDesc[v].Type;

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
                LOG_WARNING_MESSAGE("Variable '", VarName, "' labeled as ", GetShaderVariableTypeLiteralName(VarType), " not found in shader \'", ShaderName, "'");
            }
        }
    }
#endif
}

SPIRVShaderResources::SPIRVShaderResources(IMemoryAllocator &Allocator,
                                           const SPIRVShaderResources& SrcResources,
                                           const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                                           Uint32 NumAllowedTypes) : 
    m_MemoryBuffer(nullptr, STDDeleterRawMem<void>(Allocator)),
    m_ShaderType(SrcResources.m_ShaderType)
{
    Uint32 NumUBs = 0, NumSBs = 0, NumImgs = 0, NumSmplImgs = 0, NumACs = 0, NumSepImgs = 0, NumSepSmpls = 0;
    SrcResources.CountResources(AllowedVarTypes, NumAllowedTypes, NumUBs, NumSBs, NumImgs, NumSmplImgs, NumACs, NumSepImgs, NumSepSmpls);

    Initialize(Allocator, NumUBs, NumSBs, NumImgs, NumSmplImgs, NumACs, NumSepImgs, NumSepSmpls);
    
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

    Uint32 CurrUB = 0, CurrSB = 0, CurrImg = 0, CurrSmplImg = 0, CurrAC = 0, CurrSepImg = 0, CurrSepSmpl = 0;
    SrcResources.ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const SPIRVShaderResourceAttribs &UB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(UB.VarType, AllowedTypeBits));
            new (&GetUB(CurrUB++)) SPIRVShaderResourceAttribs(UB);
        },
        [&](const SPIRVShaderResourceAttribs& SB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SB.VarType, AllowedTypeBits));
            new (&GetSB(CurrSB++)) SPIRVShaderResourceAttribs(SB);
        },
        [&](const SPIRVShaderResourceAttribs &Img, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(Img.VarType, AllowedTypeBits));
            new (&GetImg(CurrImg++)) SPIRVShaderResourceAttribs(Img);
        },
        [&](const SPIRVShaderResourceAttribs &SmplImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SmplImg.VarType, AllowedTypeBits));
            new (&GetSmplImg(CurrSmplImg++)) SPIRVShaderResourceAttribs(SmplImg);
        },
        [&](const SPIRVShaderResourceAttribs &AC, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(AC.VarType, AllowedTypeBits));
            new (&GetAC(CurrAC++)) SPIRVShaderResourceAttribs(AC);
        },
        [&](const SPIRVShaderResourceAttribs &SepImg, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepImg.VarType, AllowedTypeBits));
            new (&GetSepImg(CurrSepImg++)) SPIRVShaderResourceAttribs(SepImg);
        },
        [&](const SPIRVShaderResourceAttribs &SepSmpl, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(SepSmpl.VarType, AllowedTypeBits));
            new (&GetSepSmpl(CurrSepSmpl++)) SPIRVShaderResourceAttribs(SepSmpl);
        }
    );

    VERIFY_EXPR(CurrUB      == NumUBs);
    VERIFY_EXPR(CurrSB      == NumSBs);
    VERIFY_EXPR(CurrImg     == NumImgs);
    VERIFY_EXPR(CurrSmplImg == NumSmplImgs);
    VERIFY_EXPR(CurrAC      == NumACs);
    VERIFY_EXPR(CurrSepImg  == NumSepImgs);
    VERIFY_EXPR(CurrSepSmpl == NumSepSmpls);
}

void SPIRVShaderResources::Initialize(IMemoryAllocator &Allocator, Uint32 NumUBs, Uint32 NumSBs, Uint32 NumImgs, Uint32 NumSmplImgs, Uint32 NumACs, Uint32 NumSepImgs, Uint32 NumSepSmpls)
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
    m_BufferEndOffset = m_SeparateSamplerOffset + static_cast<OffsetType>(NumSepSmpls);

    auto MemorySize = m_BufferEndOffset * sizeof(SPIRVShaderResourceAttribs);

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
