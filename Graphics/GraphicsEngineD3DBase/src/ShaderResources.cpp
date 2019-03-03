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

#include "pch.h"
#include "EngineMemory.h"
#include "StringTools.h"
#include "ShaderResources.h"
#include "HashUtils.h"
#include "ShaderResourceVariableBase.h"

namespace Diligent
{

SHADER_RESOURCE_VARIABLE_TYPE D3DShaderResourceAttribs::FindVariableType(SHADER_TYPE                       ShaderType,
                                                                         const PipelineResourceLayoutDesc& ResourceLayoutDesc,
                                                                         const char*                       CombinedSamplerSuffix)const
{
    if (GetInputType() == D3D_SIT_SAMPLER)
    {
        // Only use CombinedSamplerSuffix when looking for the sampler variable type
        return GetShaderVariableType(ShaderType, ResourceLayoutDesc.DefaultVariableType, ResourceLayoutDesc.Variables, ResourceLayoutDesc.NumVariables,
                                     [&](const char* VarName)
                                     {
                                         return StreqSuff(Name, VarName, CombinedSamplerSuffix);
                                     });
    }
    else
    {
        return GetShaderVariableType(ShaderType, Name, ResourceLayoutDesc);
    }
}

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

void ShaderResources::AllocateMemory(IMemoryAllocator&                Allocator, 
                                     const D3DShaderResourceCounters& ResCounters,
                                     size_t                           ResourceNamesPoolSize)
{
    Uint32 CurrentOffset = 0;
    auto AdvanceOffset = [&CurrentOffset](Uint32 NumResources)
    {
        constexpr Uint32 MaxOffset = std::numeric_limits<OffsetType>::max();
        VERIFY(CurrentOffset <= MaxOffset, "Current offser (", CurrentOffset, ") exceeds max allowed value (", MaxOffset, ")");
        auto Offset = static_cast<OffsetType>(CurrentOffset);
        CurrentOffset += NumResources;
        return Offset;
    };

    auto CBOffset    = AdvanceOffset(ResCounters.NumCBs);       CBOffset; // To suppress warning
    m_TexSRVOffset   = AdvanceOffset(ResCounters.NumTexSRVs);
    m_TexUAVOffset   = AdvanceOffset(ResCounters.NumTexUAVs);
    m_BufSRVOffset   = AdvanceOffset(ResCounters.NumBufSRVs);
    m_BufUAVOffset   = AdvanceOffset(ResCounters.NumBufUAVs);
    m_SamplersOffset = AdvanceOffset(ResCounters.NumSamplers);
    m_TotalResources = AdvanceOffset(0);

    auto MemorySize = m_TotalResources * sizeof(D3DShaderResourceAttribs) + ResourceNamesPoolSize * sizeof(char);

    VERIFY_EXPR(GetNumCBs()     == ResCounters.NumCBs);
    VERIFY_EXPR(GetNumTexSRV()  == ResCounters.NumTexSRVs);
    VERIFY_EXPR(GetNumTexUAV()  == ResCounters.NumTexUAVs);
    VERIFY_EXPR(GetNumBufSRV()  == ResCounters.NumBufSRVs);
    VERIFY_EXPR(GetNumBufUAV()  == ResCounters.NumBufUAVs);
    VERIFY_EXPR(GetNumSamplers()== ResCounters.NumSamplers);

    if( MemorySize )
    {
        auto* pRawMem = ALLOCATE(Allocator, "Allocator for shader resources", MemorySize );
        m_MemoryBuffer = std::unique_ptr< void, STDDeleterRawMem<void> >(pRawMem, Allocator);
        char* NamesPool = reinterpret_cast<char*>(reinterpret_cast<D3DShaderResourceAttribs*>(pRawMem) + m_TotalResources);
        m_ResourceNames.AssignMemory(NamesPool, ResourceNamesPoolSize);
    }    
}

ShaderResources::ShaderResources(SHADER_TYPE ShaderType):
    m_ShaderType(ShaderType)
{
}


D3DShaderResourceCounters ShaderResources::CountResources(const PipelineResourceLayoutDesc&    ResourceLayout,
                                                          SHADER_TYPE                          ShaderStage,
                                                          const char*                          CombinedSamplerSuffix,
                                                          const SHADER_RESOURCE_VARIABLE_TYPE* AllowedVarTypes,
                                                          Uint32                               NumAllowedTypes)const noexcept
{
    auto AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

    D3DShaderResourceCounters Counters;
    ProcessResources(
        [&](const D3DShaderResourceAttribs& CB, Uint32)
        {
            auto VarType = CB.FindVariableType(ShaderStage, ResourceLayout, CombinedSamplerSuffix);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumCBs;
        },
        [&](const D3DShaderResourceAttribs& Sam, Uint32)
        {
            auto VarType = Sam.FindVariableType(ShaderStage, ResourceLayout, CombinedSamplerSuffix);
            if (IsAllowedType(VarType, AllowedTypeBits))
            {
                // Skip static samplers
                //if (!Sam.IsStaticSampler())
                    ++Counters.NumSamplers;
            }
        },
        [&](const D3DShaderResourceAttribs& TexSRV, Uint32)
        {
            auto VarType = TexSRV.FindVariableType(ShaderStage, ResourceLayout, CombinedSamplerSuffix);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumTexSRVs;
        },
        [&](const D3DShaderResourceAttribs& TexUAV, Uint32)
        {
            auto VarType = TexUAV.FindVariableType(ShaderStage, ResourceLayout, CombinedSamplerSuffix);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumTexUAVs;
        },
        [&](const D3DShaderResourceAttribs& BufSRV, Uint32)
        {
            auto VarType = BufSRV.FindVariableType(ShaderStage, ResourceLayout, CombinedSamplerSuffix);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumBufSRVs;
        },
        [&](const D3DShaderResourceAttribs& BufUAV, Uint32)
        {
            auto VarType = BufUAV.FindVariableType(ShaderStage, ResourceLayout, CombinedSamplerSuffix);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumBufUAVs;
        }
    );

    return Counters;
}



Uint32 ShaderResources::FindAssignedSamplerId(const D3DShaderResourceAttribs& TexSRV, const char* SamplerSuffix)const
{
    VERIFY_EXPR(SamplerSuffix != nullptr && *SamplerSuffix != 0);
    VERIFY_EXPR(TexSRV.GetInputType() == D3D_SIT_TEXTURE && TexSRV.GetSRVDimension() != D3D_SRV_DIMENSION_BUFFER);
    auto NumSamplers = GetNumSamplers();
    for (Uint32 s = 0; s < NumSamplers; ++s)
    {
        const auto& Sampler = GetSampler(s);
        if ( StreqSuff(Sampler.Name, TexSRV.Name, SamplerSuffix) )
        {
            DEV_CHECK_ERR(Sampler.BindCount == TexSRV.BindCount || Sampler.BindCount == 1, "Sampler '", Sampler.Name, "' assigned to texture '", TexSRV.Name, "' must be scalar or have the same array dimension (", TexSRV.BindCount, "). Actual sampler array dimension : ",  Sampler.BindCount);
            return s;
        }
    }
    return D3DShaderResourceAttribs::InvalidSamplerId;
}

bool ShaderResources::IsCompatibleWith(const ShaderResources &Res)const
{
    if (GetNumCBs()      != Res.GetNumCBs()    ||
        GetNumTexSRV()   != Res.GetNumTexSRV() ||
        GetNumTexUAV()   != Res.GetNumTexUAV() ||
        GetNumBufSRV()   != Res.GetNumBufSRV() ||
        GetNumBufUAV()   != Res.GetNumBufUAV() ||
        GetNumSamplers() != Res.GetNumSamplers())
        return false;

    bool IsCompatible = true;
    ProcessResources(
        [&](const D3DShaderResourceAttribs& CB, Uint32 n)
        {
            if (!CB.IsCompatibleWith(Res.GetCB(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs& Sam, Uint32 n)
        {
            if (!Sam.IsCompatibleWith(Res.GetSampler(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs& TexSRV, Uint32 n)
        {
            if (!TexSRV.IsCompatibleWith(Res.GetTexSRV(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs& TexUAV, Uint32 n)
        {
            if (!TexUAV.IsCompatibleWith(Res.GetTexUAV(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs& BufSRV, Uint32 n)
        {
            if (!BufSRV.IsCompatibleWith(Res.GetBufSRV(n)))
                IsCompatible = false;
        },
        [&](const D3DShaderResourceAttribs& BufUAV, Uint32 n)
        {
            if (!BufUAV.IsCompatibleWith(Res.GetBufUAV(n)))
                IsCompatible = false;
        }
    );
    return IsCompatible;
}

size_t ShaderResources::GetHash()const
{
    size_t hash = ComputeHash(GetNumCBs(), GetNumTexSRV(), GetNumTexUAV(), GetNumBufSRV(), GetNumBufUAV(), GetNumSamplers());
    ProcessResources(
        [&](const D3DShaderResourceAttribs& CB, Uint32)
        {
            HashCombine(hash, CB);
        },
        [&](const D3DShaderResourceAttribs& Sam, Uint32)
        {
            HashCombine(hash, Sam);
        },
        [&](const D3DShaderResourceAttribs& TexSRV, Uint32)
        {
            HashCombine(hash, TexSRV);
        },
        [&](const D3DShaderResourceAttribs& TexUAV, Uint32)
        {
            HashCombine(hash, TexUAV);
        },
        [&](const D3DShaderResourceAttribs& BufSRV, Uint32)
        {
            HashCombine(hash, BufSRV);
        },
        [&](const D3DShaderResourceAttribs& BufUAV, Uint32)
        {
            HashCombine(hash, BufUAV);
        }
    );

    return hash;
}

}
