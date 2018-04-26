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

namespace Diligent
{

SPIRVShaderResources::SPIRVShaderResources(IMemoryAllocator &Allocator, SHADER_TYPE ShaderType, std::vector<uint32_t> spirv_binary) :
    m_MemoryBuffer(nullptr, STDDeleterRawMem<void>(Allocator)),
    m_ShaderType(ShaderType)
{
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
               static_cast<Uint32>(resources.separate_samplers.size())
               );

    for (const auto &UB : resources.uniform_buffers)
    {
        UB.name;
        unsigned location = Compiler.get_decoration(UB.id, spv::DecorationLocation);
        unsigned binding = Compiler.get_decoration(UB.id, spv::DecorationBinding);

        int a=0;
    }

    for (const auto &SB : resources.storage_buffers)
    {
        SB.name;
        unsigned location = Compiler.get_decoration(SB.id, spv::DecorationLocation);
        unsigned binding = Compiler.get_decoration(SB.id, spv::DecorationBinding);

        int a = 0;
    }

    for (const auto &SmplImg : resources.sampled_images)
    {
        SmplImg.name;
        unsigned location = Compiler.get_decoration(SmplImg.id, spv::DecorationLocation);
        unsigned binding = Compiler.get_decoration(SmplImg.id, spv::DecorationBinding);

        int a = 0;
    }

    for (const auto &Img : resources.storage_images)
    {
        Img.name;
        unsigned location = Compiler.get_decoration(Img.id, spv::DecorationLocation);
        unsigned binding = Compiler.get_decoration(Img.id, spv::DecorationBinding);

        int a = 0;
    }

    for (const auto &AC : resources.atomic_counters)
    {
        AC.name;
        unsigned location = Compiler.get_decoration(AC.id, spv::DecorationLocation);
        unsigned binding = Compiler.get_decoration(AC.id, spv::DecorationBinding);

        int a = 0;
    }

    for (const auto &SepImg : resources.separate_images)
    {
        SepImg.name;
        unsigned location = Compiler.get_decoration(SepImg.id, spv::DecorationLocation);
        unsigned binding = Compiler.get_decoration(SepImg.id, spv::DecorationBinding);

        int a = 0;
    }

    for (const auto &SepSam : resources.separate_samplers)
    {
        SepSam.name;
        unsigned location = Compiler.get_decoration(SepSam.id, spv::DecorationLocation);
        unsigned binding = Compiler.get_decoration(SepSam.id, spv::DecorationBinding);

        int a = 0;
    }
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
