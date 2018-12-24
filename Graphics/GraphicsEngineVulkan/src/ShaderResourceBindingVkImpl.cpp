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

#include <array>
#include "pch.h"
#include "ShaderResourceBindingVkImpl.h"
#include "PipelineStateVkImpl.h"
#include "ShaderVkImpl.h"
#include "RenderDeviceVkImpl.h"

namespace Diligent
{

ShaderResourceBindingVkImpl::ShaderResourceBindingVkImpl( IReferenceCounters* pRefCounters, PipelineStateVkImpl* pPSO, bool IsPSOInternal) :
    TBase( pRefCounters, pPSO, IsPSOInternal ),
    m_ShaderResourceCache(ShaderResourceCacheVk::DbgCacheContentType::SRBResources)
{
    auto* ppShaders = pPSO->GetShaders();
    m_NumShaders = static_cast<decltype(m_NumShaders)>(pPSO->GetNumShaders());

    auto* pRenderDeviceVkImpl = pPSO->GetDevice();
    // This will only allocate memory and initialize descriptor sets in the resource cache
    // Resources will be initialized by InitializeResourceMemoryInCache()
    auto& ResourceCacheDataAllocator = pPSO->GetSRBMemoryAllocator().GetResourceCacheDataAllocator(0);
    pPSO->GetPipelineLayout().InitResourceCache(pRenderDeviceVkImpl, m_ShaderResourceCache, ResourceCacheDataAllocator, pPSO->GetDesc().Name);
    
    auto *pVarMgrsRawMem = ALLOCATE(GetRawAllocator(), "Raw memory for ShaderVariableManagerVk", m_NumShaders * sizeof(ShaderVariableManagerVk));
    m_pShaderVarMgrs = reinterpret_cast<ShaderVariableManagerVk*>(pVarMgrsRawMem);

    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto *pShader = ppShaders[s];
        auto ShaderType = pShader->GetDesc().ShaderType;
        auto ShaderInd = GetShaderTypeIndex(ShaderType);
        
        auto &VarDataAllocator = pPSO->GetSRBMemoryAllocator().GetShaderVariableDataAllocator(s);

        const auto &SrcLayout = pPSO->GetShaderResLayout(s);
        // Use source layout to initialize resource memory in the cache
        SrcLayout.InitializeResourceMemoryInCache(m_ShaderResourceCache);

        // Create shader variable manager in place
        new (m_pShaderVarMgrs + s) ShaderVariableManagerVk(*this);
        
        // Initialize vars manager to reference mutable and dynamic variables
        // Note that the cache has space for all variable types
        std::array<SHADER_VARIABLE_TYPE, 2> VarTypes = {{SHADER_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_TYPE_DYNAMIC}};
        m_pShaderVarMgrs[s].Initialize(SrcLayout, VarDataAllocator, VarTypes.data(), static_cast<Uint32>(VarTypes.size()), m_ShaderResourceCache);
        
        m_ResourceLayoutIndex[ShaderInd] = static_cast<Int8>(s);
    }
}

ShaderResourceBindingVkImpl::~ShaderResourceBindingVkImpl()
{
    PipelineStateVkImpl* pPSO = ValidatedCast<PipelineStateVkImpl>(m_pPSO);
    for(Uint32 s = 0; s < m_NumShaders; ++s)
    {
        auto &VarDataAllocator = pPSO->GetSRBMemoryAllocator().GetShaderVariableDataAllocator(s);
        m_pShaderVarMgrs[s].Destroy(VarDataAllocator);
        m_pShaderVarMgrs[s].~ShaderVariableManagerVk();
    }

    GetRawAllocator().Free(m_pShaderVarMgrs);
}

IMPLEMENT_QUERY_INTERFACE( ShaderResourceBindingVkImpl, IID_ShaderResourceBindingVk, TBase )

void ShaderResourceBindingVkImpl::BindResources(Uint32 ShaderFlags, IResourceMapping *pResMapping, Uint32 Flags)
{
    for (auto ShaderInd = 0; ShaderInd <= CSInd; ++ShaderInd )
    {
        if (ShaderFlags & GetShaderTypeFromIndex(ShaderInd))
        {
            auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
            if(ResLayoutInd >= 0)
            {
                m_pShaderVarMgrs[ResLayoutInd].BindResources(pResMapping, Flags);
            }
        }
    }
}

IShaderVariable* ShaderResourceBindingVkImpl::GetVariable(SHADER_TYPE ShaderType, const char *Name)
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
    if (ResLayoutInd < 0)
    {
        LOG_WARNING_MESSAGE("Unable to find mutable/dynamic variable '", Name, "': shader stage ", GetShaderTypeLiteralName(ShaderType), " is inactive");
        return nullptr;
    }
    return m_pShaderVarMgrs[ResLayoutInd].GetVariable(Name);
}

Uint32 ShaderResourceBindingVkImpl::GetVariableCount(SHADER_TYPE ShaderType) const
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
    if (ResLayoutInd < 0)
    {
        LOG_WARNING_MESSAGE("Unable to get the number of mutable/dynamic variables: shader stage ", GetShaderTypeLiteralName(ShaderType), " is inactive");
        return 0;
    }
    return m_pShaderVarMgrs[ResLayoutInd].GetVariableCount();
}

IShaderVariable* ShaderResourceBindingVkImpl::GetVariable(SHADER_TYPE ShaderType, Uint32 Index)
{
    auto ShaderInd = GetShaderTypeIndex(ShaderType);
    auto ResLayoutInd = m_ResourceLayoutIndex[ShaderInd];
    if (ResLayoutInd < 0)
    {
        LOG_ERROR("Unable to get mutable/dynamic variable at index ", Index, ": shader stage ", GetShaderTypeLiteralName(ShaderType), " is inactive");
        return nullptr;
    }
    return m_pShaderVarMgrs[ResLayoutInd].GetVariable(Index);
}

void ShaderResourceBindingVkImpl::InitializeStaticResources(const IPipelineState* pPipelineState)
{
    if (StaticResourcesInitialized())
    {
        LOG_WARNING_MESSAGE("Static resources have already been initialized in this shader resource binding object. The operation will be ignored.");
        return;
    }

    if (pPipelineState == nullptr)
    {
        pPipelineState = GetPipelineState();
    }
    else
    {
        DEV_CHECK_ERR(pPipelineState->IsCompatibleWith(GetPipelineState()), "The pipeline state is not compatible with this SRB");
    }

    auto* pPSOVK = ValidatedCast<const PipelineStateVkImpl>(pPipelineState);
    for (Uint32 s = 0; s < m_NumShaders; ++s)
    {
        const auto* pShaderVk = pPSOVK->GetShader<const ShaderVkImpl>(s);
#ifdef DEVELOPMENT
        if (!pShaderVk->DvpVerifyStaticResourceBindings())
        {
            LOG_ERROR_MESSAGE("Static resources in SRB of PSO '", pPSOVK->GetDesc().Name, "' will not be successfully initialized "
                              "because not all static resource bindings in shader '", pShaderVk->GetDesc().Name, "' are valid. "
                              "Please make sure you bind all static resources to the shader before calling InitializeStaticResources() "
                              "directly or indirectly by passing InitStaticResources=true to CreateShaderResourceBinding() method.");
        }
#endif
        const auto& StaticResLayout = pShaderVk->GetStaticResLayout();
        const auto& StaticResCache = pShaderVk->GetStaticResCache();
        const auto& ShaderResourceLayouts = pPSOVK->GetShaderResLayout(s);
        ShaderResourceLayouts.InitializeStaticResources(StaticResLayout, StaticResCache, m_ShaderResourceCache);
    }

    m_bStaticResourcesInitialized = true;
}

}
