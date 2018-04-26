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

SPIRVShaderResources::SPIRVShaderResources(IMemoryAllocator &Allocator, SHADER_TYPE ShaderType) :
    m_MemoryBuffer(nullptr, STDDeleterRawMem<void>(Allocator)),
    m_ShaderType(ShaderType)
{
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
    NumSepImgs = 0;
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

void SPIRVShaderResources::Load(std::vector<uint32_t> spirv_binary)
{
    spirv_cross::Compiler Compiler(std::move(spirv_binary));

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = Compiler.get_shader_resources();

    for(const auto &UB : resources.uniform_buffers)
    {

    }

    for (const auto &SB : resources.storage_buffers)
    {

    }

    for (const auto &SmplImg : resources.sampled_images)
    {

    }

    for (const auto &Img : resources.storage_images)
    {

    }

    for (const auto &AC : resources.atomic_counters)
    {

    }

    for (const auto &SepImg : resources.separate_images)
    {

    }

    for (const auto &SepSam : resources.separate_samplers)
    {

    }
}

}
