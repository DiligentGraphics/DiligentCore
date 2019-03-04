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

    auto CBOffset    = AdvanceOffset(ResCounters.NumCBs);       (void)CBOffset; // To suppress warning
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

SHADER_RESOURCE_VARIABLE_TYPE ShaderResources::FindVariableType(const D3DShaderResourceAttribs&   ResourceAttribs,
                                                                const PipelineResourceLayoutDesc& ResourceLayout)const
{
    if (ResourceAttribs.GetInputType() == D3D_SIT_SAMPLER)
    {
        // Only use CombinedSamplerSuffix when looking for the sampler variable type
        return GetShaderVariableType(m_ShaderType, ResourceLayout.DefaultVariableType, ResourceLayout.Variables, ResourceLayout.NumVariables,
                                     [&](const char* VarName)
                                     {
                                         return StreqSuff(ResourceAttribs.Name, VarName, m_SamplerSuffix);
                                     });
    }
    else
    {
        return GetShaderVariableType(m_ShaderType, ResourceAttribs.Name, ResourceLayout);
    }
}

Int32 ShaderResources::FindStaticSampler(const D3DShaderResourceAttribs&   ResourceAttribs,
                                         const PipelineResourceLayoutDesc& ResourceLayoutDesc)const
{
    VERIFY(ResourceAttribs.GetInputType() == D3D_SIT_SAMPLER, "Sampler is expected");

    for (Uint32 s=0; s < ResourceLayoutDesc.NumStaticSamplers; ++s)
    {
        const auto& StSam = ResourceLayoutDesc.StaticSamplers[s];
        if ( ((StSam.ShaderStages & m_ShaderType) != 0) && StreqSuff(ResourceAttribs.Name, StSam.SamplerOrTextureName, m_SamplerSuffix) )
            return s;
    }

    return -1;
}


D3DShaderResourceCounters ShaderResources::CountResources(const PipelineResourceLayoutDesc&    ResourceLayout,
                                                          const SHADER_RESOURCE_VARIABLE_TYPE* AllowedVarTypes,
                                                          Uint32                               NumAllowedTypes)const noexcept
{
    auto AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);

    D3DShaderResourceCounters Counters;
    ProcessResources(
        [&](const D3DShaderResourceAttribs& CB, Uint32)
        {
            auto VarType = FindVariableType(CB, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumCBs;
        },
        [&](const D3DShaderResourceAttribs& Sam, Uint32)
        {
            auto VarType = FindVariableType(Sam, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
                    ++Counters.NumSamplers;
        },
        [&](const D3DShaderResourceAttribs& TexSRV, Uint32)
        {
            auto VarType = FindVariableType(TexSRV, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumTexSRVs;
        },
        [&](const D3DShaderResourceAttribs& TexUAV, Uint32)
        {
            auto VarType = FindVariableType(TexUAV, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumTexUAVs;
        },
        [&](const D3DShaderResourceAttribs& BufSRV, Uint32)
        {
            auto VarType = FindVariableType(BufSRV, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumBufSRVs;
        },
        [&](const D3DShaderResourceAttribs& BufUAV, Uint32)
        {
            auto VarType = FindVariableType(BufUAV, ResourceLayout);
            if (IsAllowedType(VarType, AllowedTypeBits))
                ++Counters.NumBufUAVs;
        }
    );

    return Counters;
}

#ifdef DEVELOPMENT
void ShaderResources::DvpVerifyResourceLayout(const PipelineResourceLayoutDesc& ResourceLayout)const
{
    const auto UseCombinedTextureSamplers = IsUsingCombinedTextureSamplers();
    for (Uint32 v = 0; v < ResourceLayout.NumVariables; ++v)
    {
        const auto& VarDesc = ResourceLayout.Variables[v];
        if (VarDesc.ShaderStages == SHADER_TYPE_UNKNOWN)
        {
            LOG_WARNING_MESSAGE("No allowed shader stages are specified for ", GetShaderVariableTypeLiteralName(VarDesc.Type), " variable '", VarDesc.Name, "'.");
            continue;
        }

        if( (VarDesc.ShaderStages & m_ShaderType) == 0)
            continue;

        bool VariableFound = false;
        for (Uint32 n=0; n < m_TotalResources && !VariableFound; ++n)
        {
            const auto& Res = GetResAttribs(n, m_TotalResources, 0);

            // Skip samplers if combined texture samplers are used as 
            // in this case they are not treated as independent variables
            if (UseCombinedTextureSamplers && Res.GetInputType() == D3D_SIT_SAMPLER)
                continue;

            VariableFound = (strcmp(Res.Name, VarDesc.Name) == 0);
        }

        if(!VariableFound)
        {
            LOG_WARNING_MESSAGE("Variable '", VarDesc.Name, "' is not found in shader '", m_ShaderName, '\'');
        }
    }

    for (Uint32 s = 0; s < ResourceLayout.NumStaticSamplers; ++s)
    {
        const auto& StSamDesc = ResourceLayout.StaticSamplers[s];
        if (StSamDesc.ShaderStages == SHADER_TYPE_UNKNOWN)
        {
            LOG_WARNING_MESSAGE("No allowed shader stages are specified for static sampler '", StSamDesc.SamplerOrTextureName, "'.");
            continue;
        }

        if ( (StSamDesc.ShaderStages & m_ShaderType) == 0)
            continue;

        const auto* TexOrSamName = StSamDesc.SamplerOrTextureName;

        if (UseCombinedTextureSamplers)
        {
            bool TextureFound = false;
            for(Uint32 n=0; n < GetNumTexSRV() && !TextureFound; ++n)
            {
                const auto& TexSRV = GetTexSRV(n);
                TextureFound = (strcmp(TexSRV.Name, TexOrSamName) == 0);
            }
            if (!TextureFound)
            {
                LOG_WARNING_MESSAGE("Static sampler specifies a texture '", TexOrSamName, "' that is not found in shader '", m_ShaderName, '\'');
            }
        }
        else
        {
            bool SamplerFound = false;
            for(Uint32 n=0; n < GetNumSamplers() && !SamplerFound; ++n)
            {
                const auto& Sampler = GetSampler(n);
                SamplerFound = (strcmp(Sampler.Name, TexOrSamName) == 0);
            }
            if (!SamplerFound)
                LOG_WARNING_MESSAGE("Static sampler '", TexOrSamName, "' is not found in shader '", m_ShaderName, '\'');
        }
    }
}
#endif


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

ShaderResourceDesc ShaderResources::GetShaderResourceDesc(Uint32 Index)const
{
    DEV_CHECK_ERR(Index < m_TotalResources, "Resource index (", Index, ") is out of range");
    ShaderResourceDesc ResourceDesc;
    if (Index < m_TotalResources)
    {
        const auto& Res = GetResAttribs(Index, 0, m_TotalResources);
        ResourceDesc.Name      = Res.Name;
        ResourceDesc.ArraySize = Res.BindCount;
        switch(Res.GetInputType())
        {
            case D3D_SIT_CBUFFER:
                ResourceDesc.Type = SHADER_RESOURCE_TYPE_CONSTANT_BUFFER;
            break;

            case D3D_SIT_TBUFFER:
                UNSUPPORTED( "TBuffers are not supported" ); 
                ResourceDesc.Type = SHADER_RESOURCE_TYPE_UNKNOWN;
            break;

            case D3D_SIT_TEXTURE:
                ResourceDesc.Type = (Res.GetSRVDimension() == D3D_SRV_DIMENSION_BUFFER ? SHADER_RESOURCE_TYPE_BUFFER_SRV : SHADER_RESOURCE_TYPE_TEXTURE_SRV);
            break;

            case D3D_SIT_SAMPLER:
                ResourceDesc.Type = SHADER_RESOURCE_TYPE_SAMPLER;
            break;

            case D3D_SIT_UAV_RWTYPED:
                ResourceDesc.Type = (Res.GetSRVDimension() == D3D_SRV_DIMENSION_BUFFER ? SHADER_RESOURCE_TYPE_BUFFER_UAV : SHADER_RESOURCE_TYPE_TEXTURE_UAV);
            break;

            case D3D_SIT_STRUCTURED:
            case D3D_SIT_BYTEADDRESS:
                ResourceDesc.Type = SHADER_RESOURCE_TYPE_BUFFER_SRV;
            break;

            case D3D_SIT_UAV_RWSTRUCTURED:
            case D3D_SIT_UAV_RWBYTEADDRESS:
            case D3D_SIT_UAV_APPEND_STRUCTURED:
            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                ResourceDesc.Type = SHADER_RESOURCE_TYPE_BUFFER_UAV;
            break;

            default:
                UNEXPECTED("Unknown input type");
        }
    }
    return ResourceDesc;
}

size_t ShaderResources::GetHash()const
{
    size_t hash = ComputeHash(GetNumCBs(), GetNumTexSRV(), GetNumTexUAV(), GetNumBufSRV(), GetNumBufUAV(), GetNumSamplers());
    for (Uint32 n=0; n < m_TotalResources; ++n)
    {
        const auto& Res = GetResAttribs(n, m_TotalResources, 0);
        HashCombine(hash, Res);
    }

    return hash;
}

}
