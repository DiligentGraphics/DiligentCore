/*     Copyright 2015-2017 Egor Yusov
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

void ShaderResources::Initialize(IMemoryAllocator &Allocator, Uint32 NumCBs, Uint32 NumTexSRVs, Uint32 NumTexUAVs, Uint32 NumBufSRVs, Uint32 NumBufUAVs, Uint32 NumSamplers)
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
    m_BufferEndOffset = m_SamplersOffset + static_cast<OffsetType>(NumSamplers);

    auto MemorySize = m_BufferEndOffset * sizeof(D3DShaderResourceAttribs);

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

        [&](const D3DShaderResourceAttribs &CB)
        {
            VERIFY_EXPR(IsAllowedType(CB.GetVariableType(), AllowedTypeBits));
            ++NumCBs;
        },
        [&](const D3DShaderResourceAttribs& TexSRV)
        {
            VERIFY_EXPR(IsAllowedType(TexSRV.GetVariableType(), AllowedTypeBits));
            ++NumTexSRVs;
            NumSamplers += TexSRV.IsValidSampler() ? 1 : 0;
        },
        [&](const D3DShaderResourceAttribs &TexUAV)
        {
            VERIFY_EXPR(IsAllowedType(TexUAV.GetVariableType(), AllowedTypeBits));
            ++NumTexUAVs;
        },
        [&](const D3DShaderResourceAttribs &BufSRV)
        {
            VERIFY_EXPR(IsAllowedType(BufSRV.GetVariableType(), AllowedTypeBits));
            ++NumBufSRVs;
        },
        [&](const D3DShaderResourceAttribs &BufUAV)
        {
            VERIFY_EXPR(IsAllowedType(BufUAV.GetVariableType(), AllowedTypeBits));
            ++NumBufUAVs;
        }
    );
}

ShaderResources::ShaderResources(IMemoryAllocator &Allocator, 
                                 const ShaderResources& SrcResources, 
                                 const SHADER_VARIABLE_TYPE *AllowedVarTypes, 
                                 Uint32 NumAllowedTypes) :
    m_MemoryBuffer(nullptr, STDDeleterRawMem<void>(Allocator)),
    m_ShaderType(SrcResources.m_ShaderType)
{
    Uint32 NumCBs = 0, NumTexSRVs = 0, NumTexUAVs = 0, NumBufSRVs = 0, NumBufUAVs = 0, NumSamplers = 0;
    SrcResources.CountResources(AllowedVarTypes, NumAllowedTypes, NumCBs, NumTexSRVs, NumTexUAVs, NumBufSRVs, NumBufUAVs, NumSamplers);

    Initialize(Allocator, NumCBs, NumTexSRVs, NumTexUAVs, NumBufSRVs, NumBufUAVs, NumSamplers);

    // In release mode, MS compiler generates this false warning:
    // Warning	C4189 'AllowedTypeBits': local variable is initialized but not referenced
    // Most likely it somehow gets confused by the variable being eliminated during function inlining
#pragma warning(push)
#pragma warning(disable : 4189)
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
#pragma warning(pop)

    Uint32 CurrCB = 0, CurrTexSRV = 0, CurrTexUAV = 0, CurrBufSRV = 0, CurrBufUAV = 0, CurrSampler = 0;
    ProcessResources(
        AllowedVarTypes, NumAllowedTypes,

        [&](const D3DShaderResourceAttribs &CB)
        {
            VERIFY_EXPR( IsAllowedType(CB.GetVariableType(), AllowedTypeBits) );
            new (&GetCB(CurrCB++)) D3DShaderResourceAttribs(CB);
        },
        [&](const D3DShaderResourceAttribs& TexSRV)
        {
            VERIFY_EXPR(IsAllowedType(TexSRV.GetVariableType(), AllowedTypeBits));
            
            auto SamplerId = D3DShaderResourceAttribs::InvalidSamplerId;
            if (TexSRV.IsValidSampler())
            {
                SamplerId = CurrSampler;
                new (&GetSampler(CurrSampler++)) D3DShaderResourceAttribs(SrcResources.GetSampler(TexSRV.GetSamplerId()));
            }
            
            new (&GetTexSRV(CurrTexSRV++)) D3DShaderResourceAttribs(TexSRV, SamplerId);
        },
        [&](const D3DShaderResourceAttribs &TexUAV)
        {
            VERIFY_EXPR( IsAllowedType(TexUAV.GetVariableType(), AllowedTypeBits) );
            new (&GetTexUAV(CurrTexUAV++)) D3DShaderResourceAttribs(TexUAV);
        },
        [&](const D3DShaderResourceAttribs &BufSRV)
        {
            VERIFY_EXPR( IsAllowedType(BufSRV.GetVariableType(), AllowedTypeBits) );
            new (&GetBufSRV(CurrBufSRV++)) D3DShaderResourceAttribs(BufSRV);
        },
        [&](const D3DShaderResourceAttribs &BufUAV)
        {
            VERIFY_EXPR( IsAllowedType(BufUAV.GetVariableType(), AllowedTypeBits) );
            new (&GetBufUAV(CurrBufUAV++)) D3DShaderResourceAttribs(BufUAV);
        }
    );

    VERIFY_EXPR(CurrCB      == NumCBs);
    VERIFY_EXPR(CurrTexSRV  == NumTexSRVs );
    VERIFY_EXPR(CurrTexUAV  == NumTexUAVs );
    VERIFY_EXPR(CurrBufSRV  == NumBufSRVs );
    VERIFY_EXPR(CurrBufUAV  == NumBufUAVs );
    VERIFY_EXPR(CurrSampler == NumSamplers );
}

Uint32 ShaderResources::FindAssignedSamplerId(const D3DShaderResourceAttribs& TexSRV)const
{
    VERIFY_EXPR(TexSRV.GetInputType() == D3D_SIT_TEXTURE);
    auto NumSamplers = GetNumSamplers();
    for (Uint32 s = 0; s < NumSamplers; ++s)
    {
        const auto &Sampler = GetSampler(s);
        if( StrCmpSuff(Sampler.Name.c_str(), TexSRV.Name.c_str(), D3DSamplerSuffix) )
        {
            VERIFY(Sampler.GetVariableType() == TexSRV.GetVariableType(), "Inconsistent texture and sampler variable types");
            VERIFY(Sampler.BindCount == TexSRV.BindCount || Sampler.BindCount == 1, "Sampler assigned to array \"", TexSRV.Name, "\" is expected to be scalar or have the same dimension (",TexSRV.BindCount,"). Actual sampler array dimension : ",  Sampler.BindCount);
            return s;
        }
    }
    return D3DShaderResourceAttribs::InvalidSamplerId;
}

}
