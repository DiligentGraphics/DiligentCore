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
#include "EngineMemory.h"
#include "StringTools.h"
#include "ShaderResources.h"
#include "HashUtils.h"

namespace Diligent
{

ShaderResources::~ShaderResources()
{
    for(Uint32 n=0; n < GetNumCBs(); ++n)
        GetCB(n).~D3DShaderResourceAttribs();

    for(Uint32 n=0; n < GetNumTexSRV(); ++n)
        GetTexSRV(n).~D3DShaderResourceAttribs();

    for(Uint32 n=0; n < GetNumTexUAV(); ++n)
        GetTexUAV(n).~D3DShaderResourceAttribs();

    for(Uint32 n=0; n < GetNumBufSRV(); ++n)
        GetBufSRV(n).~D3DShaderResourceAttribs();

    for(Uint32 n=0; n < GetNumBufUAV(); ++n)
        GetBufUAV(n).~D3DShaderResourceAttribs();

    for(Uint32 n=0; n < GetNumSamplers(); ++n)
        GetSampler(n).~D3DShaderResourceAttribs();
}

void ShaderResources::Initialize(IMemoryAllocator& Allocator, 
                                 Uint32            NumCBs, 
                                 Uint32            NumTexSRVs, 
                                 Uint32            NumTexUAVs, 
                                 Uint32            NumBufSRVs, 
                                 Uint32            NumBufUAVs, 
                                 Uint32            NumSamplers,
                                 size_t            ResourceNamesPoolSize)
{
    VERIFY( &m_MemoryBuffer.get_deleter().m_Allocator == &Allocator, "Incosistent allocators provided");

    const auto MaxOffset = static_cast<Uint32>(std::numeric_limits<OffsetType>::max());
    VERIFY(NumCBs <= MaxOffset, "Max offset exceeded");
    m_TexSRVOffset    = 0                + static_cast<OffsetType>(NumCBs);

    VERIFY(m_TexSRVOffset   + NumTexSRVs <= MaxOffset, "Max offset exceeded");
    m_TexUAVOffset    = m_TexSRVOffset   + static_cast<OffsetType>(NumTexSRVs);

    VERIFY(m_TexUAVOffset   + NumTexUAVs <= MaxOffset, "Max offset exceeded");
    m_BufSRVOffset    = m_TexUAVOffset   + static_cast<OffsetType>(NumTexUAVs);

    VERIFY(m_BufSRVOffset   + NumBufSRVs <= MaxOffset, "Max offset exceeded");
    m_BufUAVOffset    = m_BufSRVOffset   + static_cast<OffsetType>(NumBufSRVs);

    VERIFY(m_BufUAVOffset   + NumBufUAVs <= MaxOffset, "Max offset exceeded");
    m_SamplersOffset  = m_BufUAVOffset   + static_cast<OffsetType>(NumBufUAVs);

    VERIFY(m_SamplersOffset + NumSamplers<= MaxOffset, "Max offset exceeded");
    m_TotalResources = m_SamplersOffset + static_cast<OffsetType>(NumSamplers);

    auto MemorySize = m_TotalResources * sizeof(D3DShaderResourceAttribs) + ResourceNamesPoolSize * sizeof(char);

    VERIFY_EXPR(GetNumCBs()     == NumCBs);
    VERIFY_EXPR(GetNumTexSRV()  == NumTexSRVs);
    VERIFY_EXPR(GetNumTexUAV()  == NumTexUAVs);
    VERIFY_EXPR(GetNumBufSRV()  == NumBufSRVs);
    VERIFY_EXPR(GetNumBufUAV()  == NumBufUAVs);
    VERIFY_EXPR(GetNumSamplers()== NumSamplers);

    if( MemorySize )
    {
        auto *pRawMem = ALLOCATE(Allocator, "Allocator for shader resources", MemorySize );
        m_MemoryBuffer.reset(pRawMem);
        char* NamesPool = reinterpret_cast<char*>(reinterpret_cast<D3DShaderResourceAttribs*>(pRawMem) + m_TotalResources);
        m_ResourceNames.AssignMemory(NamesPool, ResourceNamesPoolSize);
    }    
}

ShaderResources::ShaderResources(IMemoryAllocator &Allocator, SHADER_TYPE ShaderType):
    m_MemoryBuffer(nullptr, STDDeleterRawMem<void>(Allocator)),
    m_ShaderType(ShaderType)
{
}

void ShaderResources::CountResources(const SHADER_VARIABLE_TYPE *AllowedVarTypes, Uint32 NumAllowedTypes, 
                                     Uint32& NumCBs, Uint32& NumTexSRVs, Uint32& NumTexUAVs, 
                                     Uint32& NumBufSRVs, Uint32& NumBufUAVs, Uint32& NumSamplers)const noexcept
{
    // In release mode, MS compiler generates this false warning:
    // Warning	C4189 'AllowedTypeBits': local variable is initialized but not referenced
    // Most likely it somehow gets confused by the variable being eliminated during function inlining
#pragma warning(push)
#pragma warning(disable : 4189)
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
#pragma warning (pop)   

    NumCBs = 0;
    NumTexSRVs = 0;
    NumTexUAVs = 0;
    NumBufSRVs = 0;
    NumBufUAVs = 0;
    NumSamplers = 0;
    ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const D3DShaderResourceAttribs &CB, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(CB.VariableType, AllowedTypeBits));
            ++NumCBs;
        },
        [&](const D3DShaderResourceAttribs& TexSRV, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(TexSRV.VariableType, AllowedTypeBits));
            ++NumTexSRVs;
            NumSamplers += TexSRV.IsValidSampler() ? 1 : 0;
        },
        [&](const D3DShaderResourceAttribs &TexUAV, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(TexUAV.VariableType, AllowedTypeBits));
            ++NumTexUAVs;
        },
        [&](const D3DShaderResourceAttribs &BufSRV, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(BufSRV.VariableType, AllowedTypeBits));
            ++NumBufSRVs;
        },
        [&](const D3DShaderResourceAttribs &BufUAV, Uint32)
        {
            VERIFY_EXPR(IsAllowedType(BufUAV.VariableType, AllowedTypeBits));
            ++NumBufUAVs;
        }
    );
}


Uint32 ShaderResources::FindAssignedSamplerId(const D3DShaderResourceAttribs& TexSRV)const
{
    VERIFY_EXPR(TexSRV.InputType == D3D_SIT_TEXTURE);
    auto NumSamplers = GetNumSamplers();
    for (Uint32 s = 0; s < NumSamplers; ++s)
    {
        const auto &Sampler = GetSampler(s);
        if( StrCmpSuff(Sampler.Name, TexSRV.Name, D3DSamplerSuffix) )
        {
            VERIFY(Sampler.VariableType == TexSRV.VariableType, "Inconsistent texture and sampler variable types");
            VERIFY(Sampler.BindCount == TexSRV.BindCount || Sampler.BindCount == 1, "Sampler assigned to array \"", TexSRV.Name, "\" is expected to be scalar or have the same dimension (",TexSRV.BindCount,"). Actual sampler array dimension : ",  Sampler.BindCount);
            return s;
        }
    }
    return D3DShaderResourceAttribs::InvalidSamplerId;
}

bool ShaderResources::IsCompatibleWith(const ShaderResources &Res)const
{
    if (GetNumCBs()    != Res.GetNumCBs()    ||
        GetNumTexSRV() != Res.GetNumTexSRV() ||
        GetNumTexUAV() != Res.GetNumTexUAV() ||
        GetNumBufSRV() != Res.GetNumBufSRV() ||
        GetNumBufUAV() != Res.GetNumBufUAV())
        return false;

    bool IsCompatible = true;
    ProcessResources(
        nullptr, 0,

        [&](const D3DShaderResourceAttribs &CB, Uint32 n)
        {
            if(!CB.IsCompatibleWith(Res.GetCB(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs& TexSRV, Uint32 n)
        {
            if(!TexSRV.IsCompatibleWith(Res.GetTexSRV(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs &TexUAV, Uint32 n)
        {
            if(!TexUAV.IsCompatibleWith(Res.GetTexUAV(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs &BufSRV, Uint32 n)
        {
            if(!BufSRV.IsCompatibleWith(Res.GetBufSRV(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs &BufUAV, Uint32 n)
        {
            if(!BufUAV.IsCompatibleWith(Res.GetBufUAV(n)))
                IsCompatible = false;
        }
    );
    return IsCompatible;
}

size_t ShaderResources::GetHash()const
{
    size_t hash = ComputeHash(GetNumCBs(), GetNumTexSRV(), GetNumTexUAV(), GetNumBufSRV(), GetNumBufUAV());
    ProcessResources(
        nullptr, 0,
        [&](const D3DShaderResourceAttribs &CB, Uint32)
        {
            HashCombine(hash, CB);
        },
        [&](const D3DShaderResourceAttribs &TexSRV, Uint32)
        {
            HashCombine(hash, TexSRV);
        },
        [&](const D3DShaderResourceAttribs &TexUAV, Uint32)
        {
            HashCombine(hash, TexUAV);
        },
        [&](const D3DShaderResourceAttribs &BufSRV, Uint32)
        {
            HashCombine(hash, BufSRV);
        },
        [&](const D3DShaderResourceAttribs &BufUAV, Uint32)
        {
            HashCombine(hash, BufUAV);
        }
    );

    return hash;
}

}
