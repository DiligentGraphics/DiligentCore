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

#include "ShaderVariableVk.h"
#include "ShaderResourceVariableBase.h"

namespace Diligent
{

size_t ShaderVariableManagerVk::GetRequiredMemorySize(const ShaderResourceLayoutVk& Layout, 
                                                      const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes, 
                                                      Uint32                        NumAllowedTypes,
                                                      Uint32&                       NumVariables)
{
    NumVariables = 0;
    Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    for(SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType+1))
    {
        if (IsAllowedType(VarType, AllowedTypeBits))
        {
            auto NumResources = Layout.GetResourceCount(VarType);
            if (Layout.IsUsingSeparateSamplers())
                NumVariables += NumResources;
            else
            {
                // When using HLSL-style combined image samplers, we need to skip separate samplers
                for( Uint32 r=0; r < NumResources; ++r )
                {
                    const auto& SrcRes = Layout.GetResource(VarType, r);
                    if (SrcRes.SpirvAttribs.Type != SPIRVShaderResourceAttribs::ResourceType::SeparateSampler)
                        ++NumVariables;
                }
            }
        }
    }
    
    return NumVariables*sizeof(ShaderVariableVkImpl);
}

// Creates shader variable for every resource from SrcLayout whose type is one AllowedVarTypes
void ShaderVariableManagerVk::Initialize(const ShaderResourceLayoutVk& SrcLayout, 
                                         IMemoryAllocator&             Allocator,
                                         const SHADER_RESOURCE_VARIABLE_TYPE*   AllowedVarTypes, 
                                         Uint32                        NumAllowedTypes, 
                                         ShaderResourceCacheVk&        ResourceCache)
{
    m_pResourceLayout = &SrcLayout;
    m_pResourceCache  = &ResourceCache;
#ifdef _DEBUG
    m_pDbgAllocator = &Allocator;
#endif

    const Uint32 AllowedTypeBits = GetAllowedTypeBits(AllowedVarTypes, NumAllowedTypes);
    VERIFY_EXPR(m_NumVariables == 0);
    auto MemSize = GetRequiredMemorySize(SrcLayout, AllowedVarTypes, NumAllowedTypes, m_NumVariables);
    
    if(m_NumVariables == 0)
        return;
    
    auto *pRawMem = ALLOCATE(Allocator, "Raw memory buffer for shader variables", MemSize);
    m_pVariables = reinterpret_cast<ShaderVariableVkImpl*>(pRawMem);

    Uint32 VarInd = 0;
    for(SHADER_RESOURCE_VARIABLE_TYPE VarType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC; VarType < SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES; VarType = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(VarType+1))
    {
        if (!IsAllowedType(VarType, AllowedTypeBits))
            continue;

        Uint32 NumResources = SrcLayout.GetResourceCount(VarType);
        for( Uint32 r=0; r < NumResources; ++r )
        {
            const auto& SrcRes = SrcLayout.GetResource(VarType, r);
            if (!SrcLayout.IsUsingSeparateSamplers() && SrcRes.SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler)
                continue;

            ::new (m_pVariables + VarInd) ShaderVariableVkImpl(*this, SrcRes );
            ++VarInd;
        }
    }
    VERIFY_EXPR(VarInd == m_NumVariables);
}

ShaderVariableManagerVk::~ShaderVariableManagerVk()
{
    VERIFY(m_pVariables == nullptr, "Destroy() has not been called");
}

void ShaderVariableManagerVk::Destroy(IMemoryAllocator &Allocator)
{
    VERIFY(m_pDbgAllocator == &Allocator, "Incosistent alloctor");

    if(m_pVariables != nullptr)
    {
        for(Uint32 v=0; v < m_NumVariables; ++v)
            m_pVariables[v].~ShaderVariableVkImpl();
        Allocator.Free(m_pVariables);
        m_pVariables = nullptr;
    }
}

ShaderVariableVkImpl* ShaderVariableManagerVk::GetVariable(const Char* Name)
{
    ShaderVariableVkImpl* pVar = nullptr;
    for (Uint32 v = 0; v < m_NumVariables; ++v)
    {
        auto &Var = m_pVariables[v];
        const auto& Res = Var.m_Resource;
        if (strcmp(Res.SpirvAttribs.Name, Name) == 0)
        {
            pVar = &Var;
            break;
        }
    }
    return pVar;
}


ShaderVariableVkImpl* ShaderVariableManagerVk::GetVariable(Uint32 Index)
{
    if (Index >= m_NumVariables)
    {
        LOG_ERROR("Index ", Index, " is out of range");
        return nullptr;
    }

    return m_pVariables + Index;
}

Uint32 ShaderVariableManagerVk::GetVariableIndex(const ShaderVariableVkImpl& Variable)
{
    if (m_pVariables == nullptr)
    {
        LOG_ERROR("This shader variable manager has no variables");
        return static_cast<Uint32>(-1);
    }

    auto Offset = reinterpret_cast<const Uint8*>(&Variable) - reinterpret_cast<Uint8*>(m_pVariables);
    VERIFY(Offset % sizeof(ShaderVariableVkImpl) == 0, "Offset is not multiple of ShaderVariableVkImpl class size");
    auto Index = static_cast<Uint32>(Offset / sizeof(ShaderVariableVkImpl));
    if (Index < m_NumVariables)
        return Index;
    else
    {
        LOG_ERROR("Failed to get variable index. The variable ", &Variable, " does not belong to this shader variable manager");
        return static_cast<Uint32>(-1);
    }
}

void ShaderVariableManagerVk::BindResources( IResourceMapping* pResourceMapping, Uint32 Flags)
{
    VERIFY_EXPR(m_pResourceCache != nullptr);

    if( !pResourceMapping )
    {
        LOG_ERROR_MESSAGE( "Failed to bind resources: resource mapping is null" );
        return;
    }

    if ( (Flags & BIND_SHADER_RESOURCES_UPDATE_ALL) == 0 )
        Flags |= BIND_SHADER_RESOURCES_UPDATE_ALL;

    for(Uint32 v=0; v < m_NumVariables; ++v)
    {
        auto &Var = m_pVariables[v];
        const auto& Res = Var.m_Resource;
        
        // Skip immutable separate samplers
        if (Res.SpirvAttribs.Type == SPIRVShaderResourceAttribs::ResourceType::SeparateSampler && Res.IsImmutableSamplerAssigned())
            continue;

        if ( (Flags & (1 << Res.GetVariableType())) == 0 )
            continue;

        for (Uint32 ArrInd = 0; ArrInd < Res.SpirvAttribs.ArraySize; ++ArrInd)
        {
            if( (Flags & BIND_SHADER_RESOURCES_KEEP_EXISTING) && Res.IsBound(ArrInd, *m_pResourceCache) )
                continue;

            const auto* VarName = Res.SpirvAttribs.Name;
            RefCntAutoPtr<IDeviceObject> pObj;
            pResourceMapping->GetResource( VarName, &pObj, ArrInd );
            if( pObj )
            {
                Res.BindResource(pObj, ArrInd, *m_pResourceCache);
            }
            else
            {
                if( (Flags & BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED) && !Res.IsBound(ArrInd, *m_pResourceCache) )
                    LOG_ERROR_MESSAGE( "Unable to bind resource to shader variable '", Res.SpirvAttribs.GetPrintName(ArrInd), "': resource is not found in the resource mapping" );
            }
        }
    }
}

}
